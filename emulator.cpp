#include "emulator.h"
#include "display.h"
#include "ops.h"
#include "state.h"

#include <algorithm>
#include <cstdint>
#include <iostream>
#include <string>
#include <utility>
#include <vector>

#include <SDL2/SDL.h>

using std::copy;
using std::cout;
using std::pair;
using std::string;
using std::uint8_t;
using std::uint32_t;
using std::vector;

bool quit = false;

int main(int argc, char* argv[])
{
    if (argc < 2) {
	cout << "Enter ROM filename as an argument.\n";
	return 0;
    }

    if (SDL_Init(SDL_INIT_EVERYTHING) != 0) {
	cout << "SDL2 failed to initialize: " << SDL_GetError() << "\n";
	return 0;
    }

    SDL_Window* window = SDL_CreateWindow("GameBoy Emulator",
		                          SDL_WINDOWPOS_UNDEFINED, SDL_WINDOWPOS_UNDEFINED,
					  SCREEN_WIDTH, SCREEN_HEIGHT, SDL_WINDOW_SHOWN);
    if (!window) {
	cout << "Failed to create window: " << SDL_GetError() << "\n";
	return 0;
    }
    SDL_Surface* display_surface = SDL_GetWindowSurface(window);
    SDL_Surface* display_buffer = SDL_CreateRGBSurface(0, 256, 256, 32, 0, 0, 0, 0);

    State state;
    state.load_file_to_memory(argv[1]);

    state.a = 0x01; state.f = 0xb0; state.b = 0x00; state.c = 0x13;
    state.d = 0x00; state.e = 0xd8; state.h = 0x01; state.l = 0x4d;

    vector<pair<uint16_t, uint8_t>> memory_values = {
        {0xff00, 0xff}, {0xff05, 0x00}, {0xff06, 0x00}, {0xff07, 0x00},
	{0xff10, 0x80}, {0xff11, 0xbf}, {0xff12, 0xf3}, {0xff14, 0xbf},
	{0xff16, 0x3f}, {0xff17, 0x00}, {0xff19, 0xbf}, {0xff1a, 0x7f},
	{0xff1b, 0xff}, {0xff1c, 0x9f}, {0xff1e, 0xbf}, {0xff20, 0xff},
	{0xff21, 0x00}, {0xff22, 0x00}, {0xff23, 0xbf}, {0xff24, 0x77},
	{0xff25, 0xf3}, {0xff26, 0xf1}, {0xff40, 0x91}, {0xff42, 0x00},
	{0xff43, 0x00}, {0xff45, 0x00}, {0xff47, 0xfc}, {0xff48, 0xff},
	{0xff49, 0xff}, {0xff4a, 0x00}, {0xff4b, 0x00}, {0xffff, 0x00}
    };
    for (auto value : memory_values) {
        state.write_memory(value.first, value.second);
    }

    uint32_t last_time_ms = SDL_GetTicks();
    uint32_t current_time_ms = last_time_ms;
    uint32_t cycles_to_catch_up = 0;
    uint8_t draw_line_counter = 0;
    uint16_t timer_counter = 0;
    uint16_t divider_counter = 0;
    while (!quit) {
	current_time_ms = SDL_GetTicks();
	cycles_to_catch_up += (current_time_ms - last_time_ms) * 1048;
	last_time_ms = current_time_ms;
	handle_events();

	while (!quit && cycles_to_catch_up > 0) {
            uint8_t cycles_executed = 1;
            if (!state.halt_mode) {
 	        cycles_executed = execute_op(state) / 4;
	    }

            if (state.read_memory(0xff02) == 0x81) {
	        state.write_memory(0xff02, 0);
	        cout << state.read_memory(0xff01);
	    }

 	    if (cycles_executed > cycles_to_catch_up + 20) {break;}
 	    cycles_to_catch_up -= cycles_executed;
	    draw_line_counter += cycles_executed;
	    timer_counter += cycles_executed;
	    divider_counter += cycles_executed;

	    handle_events();

	    // 0xff40 is the LCD control register.
	    if ((state.read_memory(0xff40) & 0x80) == 0x80) {
		if (draw_line_counter >= 114) {
                    if (state.read_memory(0xff44) == 0) {
	                draw_display_buffer(state, display_buffer);
	             }
 
		    draw_display_line(state, display_buffer, display_surface);
		    draw_line_counter -= 114;

		    if (state.read_memory(0xff44) == 144) {
	                SDL_UpdateWindowSurface(window);
		    }
                }
	    }

            if (divider_counter >= 256) {
                divider_counter -= 256;
		state.write_memory(0xff04, state.read_memory(0xff04) + 1);
	    }

	    uint8_t timer_control = state.read_memory(0xff07);
	    if (timer_control & 0x4) {
	        uint16_t cycles = 0;
	        if ((timer_control & 0x3) == 0) {cycles = 1024;}
	        if ((timer_control & 0x3) == 1) {cycles = 16;}
	        if ((timer_control & 0x3) == 2) {cycles = 64;}
	        if ((timer_control & 0x3) == 3) {cycles = 256;}

		uint8_t timer = state.read_memory(0xff05);
		if (timer_counter >= cycles) {
                    timer_counter -= cycles;
		    timer++;
		    if (timer == 0) {
                        timer = state.read_memory(0xff06);
			state.write_memory(0xff0f, state.read_memory(0xff0f) | 0x4);
			state.halt_mode = false;
		    }
		    state.write_memory(0xff05, timer);
		}
	    }

	    handle_interrupts(state);
        }
    }

    SDL_DestroyWindow(window);
    SDL_Quit();
}

void handle_events()
{
    SDL_Event e;
    while (SDL_PollEvent(&e)) {
        if (e.type == SDL_QUIT) {
	    quit = true;
	}
    }
}

void handle_interrupts(State& state)
{
    if (!state.interrupts_enabled) {
        return;
    }
    uint8_t IF = state.read_memory(0xff0f);
    uint8_t IE = state.read_memory(0xffff);
    for (uint8_t b = 0; b < 5; b++) {
	if (IF & (1 << b) && IE & (1 << b)) {
            state.write_memory(0xff0f, IF & ~(1 << b));
	    state.interrupts_enabled = false;
	    state.halt_mode = false;
	    push_onto_stack(state, state.pc);
	    state.pc = 0x40 + 0x8 * b;
	    break;
	}
    }
}
