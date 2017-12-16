#include "emulator.h"
#include "state.h"
#include "ops.h"

#include <algorithm>
#include <cstdint>
#include <fstream>
#include <iostream>
#include <iterator>
#include <string>

#include <SDL2/SDL.h>

using std::copy;
using std::cout;
using std::ifstream;
using std::istreambuf_iterator;
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

    const uint32_t buf_size = 0x60000;
    uint8_t rom_buffer[buf_size];
    load_rom(argv[1], rom_buffer);
    State state(rom_buffer, buf_size);

    uint32_t last_time_ms = SDL_GetTicks();
    uint32_t current_time_ms = last_time_ms;
    uint32_t cycles_to_catch_up = 0;
    while (!quit) {
	current_time_ms = SDL_GetTicks();
	cycles_to_catch_up += (current_time_ms - last_time_ms) * 1048;
	last_time_ms = current_time_ms;

	while (cycles_to_catch_up > 0) {
	    break;
	    uint8_t cycles_executed = execute_op(state) / 4;
	    if (cycles_executed > cycles_to_catch_up + 20) {break;}
	    cycles_to_catch_up -= cycles_executed;
	}

	handle_events();
	SDL_UpdateWindowSurface(window);
    }

    SDL_DestroyWindow(window);
    SDL_Quit();
}

void load_rom(string filename, uint8_t* rom_buffer)
{
    ifstream rom_file(filename, ifstream::binary);
    copy(istreambuf_iterator<char>(rom_file),
         istreambuf_iterator<char>(),
	 rom_buffer);
}

void handle_events() {
    SDL_Event e;
    while (SDL_PollEvent(&e)) {
        if (e.type == SDL_QUIT) {
	    quit = true;
	}
    }
}
