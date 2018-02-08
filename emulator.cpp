#include "emulator.h"
#include "audio.h"
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

    if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_EVENTS) != 0) {
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
    SDL_Surface* display_buffer = SDL_CreateRGBSurface(0, 160, 144, 32, 0, 0, 0, 0);

    State state;
    if (!state.load_file_to_rom(argv[1])) {
        cout << "Invalid ROM filename.\n";
	return 0;
    }

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
    uint16_t event_counter = 0;
    uint16_t audio_counter = 0;
    while (!quit) {
	current_time_ms = SDL_GetTicks();
	cycles_to_catch_up += (current_time_ms - last_time_ms) * 1048;
	last_time_ms = current_time_ms;
	handle_events(state);
	if ((state.read_memory(0xff00) & 0xf) != 0xf || state.read_memory(0xff0f) & 0x10) {state.stop_mode = false;}

	while (!quit && cycles_to_catch_up > 20 && !state.stop_mode) {
            uint8_t cycles_executed = 1;
            if (!state.halt_mode) {
 	        cycles_executed = execute_op(state) / 4;
	    }

 	    cycles_to_catch_up -= cycles_executed;
	    draw_line_counter += cycles_executed;
	    timer_counter += cycles_executed;
	    divider_counter += cycles_executed;
	    event_counter += cycles_executed;
	    audio_counter += cycles_executed;
 	    if (cycles_to_catch_up < 20) {break;}

	    if (event_counter >= 100) {
	        handle_events(state);
		event_counter -= 100;
	    }

	    if (state.read_memory(0xff44) >= 144) {
		state.write_memory(0xff41, (state.read_memory(0xff41) & ~0x2) | 0x1);
	    } else if (draw_line_counter >= 63) {
		state.write_memory(0xff41, state.read_memory(0xff41) & ~0x3);
            } else if (draw_line_counter >= 20) {
		state.write_memory(0xff41, state.read_memory(0xff41) | 0x3);
            } else {
		state.write_memory(0xff41, (state.read_memory(0xff41) & ~0x1) | 0x2);
	    }

	    if (draw_line_counter >= 114) {
	        uint8_t lcdc = state.read_memory(0xff40);
	        if ((lcdc & 0x80) == 0x80) {
                    if (state.read_memory(0xff44) == 0) {
	                state.update_tile_data();
	            }
 
		    draw_display_line(state, display_buffer);
                    state.write_memory(0xff44, (state.read_memory(0xff44) + 1) % 154);
		    draw_line_counter -= 114;
		    uint8_t ly = state.read_memory(0xff44);

		    uint8_t lcd_stat = state.read_memory(0xff41);
		    bool lyc = ly == state.read_memory(0xff45);
                    if ((lcd_stat & 0x8) || (lcd_stat & 0x20) || ((lcd_stat & 0x40) && lyc)) {
			state.write_memory(0xff0f, state.read_memory(0xff0f) | 0x2);
			if (state.read_memory(0xffff) & 0x2) {
			    state.halt_mode = false;
			}
		    }
		    if (lyc) {
		        state.write_memory(0xff41, lcd_stat | 0x4);
		    } else {
		        state.write_memory(0xff41, lcd_stat & ~0x4);
		    }

		    if (ly == 144) {
			state.write_memory(0xff0f, state.read_memory(0xff0f) | 0x1);
			if (lcd_stat & 0x10) {
			    state.write_memory(0xff0f, state.read_memory(0xff0f) | 0x2);
			}
			if ((state.read_memory(0xffff) & 0x1)
				|| ((state.read_memory(0xffff) & 0x2) && (lcd_stat & 0x10))) {
			    state.halt_mode = false;
			}
			SDL_BlitScaled(display_buffer, 0, display_surface, 0);
	                SDL_UpdateWindowSurface(window);
		    }
                } else {
                    state.write_memory(0xff44, 0);
		}
	    }

            if (divider_counter >= 256) {
                divider_counter -= 256;
		state.write_memory(0xff04, state.read_memory(0xff04) + 1);
	    }

            uint8_t timer_control = state.read_memory(0xff07);
	    uint16_t cycles = 0;
	    if ((timer_control & 0x3) == 0) {cycles = 1024;}
	    if ((timer_control & 0x3) == 1) {cycles = 16;}
	    if ((timer_control & 0x3) == 2) {cycles = 64;}
	    if ((timer_control & 0x3) == 3) {cycles = 256;}

	    if (timer_control & 0x4 && timer_counter >= cycles) {
		uint8_t timer = state.read_memory(0xff05);
                timer_counter -= cycles;
		timer++;
		if (timer == 0) {
                    timer = state.read_memory(0xff06);
		    state.write_memory(0xff0f, state.read_memory(0xff0f) | 0x4);
		    if (state.read_memory(0xffff) & 0x4) {
			state.halt_mode = false;
	            }
		}
		state.write_memory(0xff05, timer);
	    }

	    uint8_t p1 = state.read_memory(0xff00);
	    if ((p1 & 0x30) == 0x20) {
                const uint8_t* keyboard = SDL_GetKeyboardState(0);
                uint8_t down = keyboard[SDL_SCANCODE_DOWN];
                uint8_t up = keyboard[SDL_SCANCODE_UP];
                uint8_t left = keyboard[SDL_SCANCODE_LEFT];
                uint8_t right = keyboard[SDL_SCANCODE_RIGHT];
		state.write_memory(0xff00, (p1 & 0xf0) | (~((down << 3) | (up << 2) | (left << 1) | right) & 0x0f));
	    } else if ((p1 & 0x30) == 0x10) {
                const uint8_t* keyboard = SDL_GetKeyboardState(0);
                uint8_t start = keyboard[SDL_SCANCODE_Z];
                uint8_t select = keyboard[SDL_SCANCODE_X];
                uint8_t b = keyboard[SDL_SCANCODE_S];
                uint8_t a = keyboard[SDL_SCANCODE_A];
		state.write_memory(0xff00, (p1 & 0xf0) | (~((start << 3) | (select << 2) | (b << 1) | a) & 0x0f));
	    } else {
		state.write_memory(0xff00, 0x3f);
	    }

	    if (audio_counter >= 100) {
                audio_counter -= 100;
		audio_controller.update_audio(state, 100);
	    }

	    handle_interrupts(state);
        }
    }

    SDL_DestroyWindow(window);
    SDL_Quit();
}

void handle_events(State& state)
{
    SDL_Event e;
    while (SDL_PollEvent(&e)) {
        if (e.type == SDL_QUIT) {
	    quit = true;
	} else if (e.type == SDL_KEYDOWN) {
            switch (e.key.keysym.sym) {
                case SDLK_DOWN:
                case SDLK_UP:
                case SDLK_LEFT:
                case SDLK_RIGHT:
                case SDLK_z:
                case SDLK_x:
                case SDLK_a:
                case SDLK_b:
		    state.write_memory(0xff0f, state.read_memory(0xff0f) | 0x10);
		    if (state.read_memory(0xffff) & 0x10) {
		        state.halt_mode = false;
		    }
                    break;
	    }
	}
    }
}

void handle_interrupts(State& state)
{
    uint8_t IF = state.read_memory(0xff0f);
    if (!state.interrupts_enabled || (IF & 0x1f) == 0) {
        return;
    }
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
