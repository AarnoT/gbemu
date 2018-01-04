#include "emulator.h"
#include "state.h"
#include "ops.h"

#include <algorithm>
#include <cstdint>
#include <iostream>
#include <string>

#include <SDL2/SDL.h>

using std::copy;
using std::cout;
using std::string;
using std::uint8_t;
using std::uint32_t;

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

    State state;
    state.load_file_to_memory(argv[1]);

    uint32_t last_time_ms = SDL_GetTicks();
    uint32_t current_time_ms = last_time_ms;
    uint32_t last_draw_time_ms = last_time_ms;
    uint32_t cycles_to_catch_up = 0;
    while (!quit) {
	current_time_ms = SDL_GetTicks();
	cycles_to_catch_up += (current_time_ms - last_time_ms) * 1048;
	last_time_ms = current_time_ms;

	while (cycles_to_catch_up > 0) {
	    uint8_t cycles_executed = execute_op(state) / 4;
	    if (cycles_executed > cycles_to_catch_up + 20) {break;}
	    cycles_to_catch_up -= cycles_executed;
	}

	handle_events();

	// 0xff40 is the LCD control register.
	if ((state.read_memory(0xff40) & 0x91) == 0x91 && 
	        current_time_ms - last_draw_time_ms > 17) {
	    SDL_UpdateWindowSurface(window);
	    last_draw_time_ms = current_time_ms;
	    // Set LY register to indicate drawing is done.
	    state.write_memory(0xff44, 0x90);
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
