#include "emulator.h"
#include "audio.h"
#include "display.h"
#include "ops.h"
#include "state.h"

#include <algorithm>
#include <cstdint>
#include <cstddef>
#include <experimental/filesystem>
#include <iostream>
#include <string>
#include <system_error>
#include <utility>
#include <vector>

#include <SDL2/SDL.h>

using std::copy;
using std::cout;
using std::experimental::filesystem::create_directory;
using std::error_code;
using std::experimental::filesystem::filesystem_error;
using std::experimental::filesystem::is_regular_file;
using std::pair;
using std::experimental::filesystem::path;
using std::size_t;
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

    string save_file_name = argv[1];
    save_file_name = "saves/" + path(save_file_name).stem().string() + ".sav";

    try {
        create_directory("saves");
	if (is_regular_file(save_file_name)) {
	    state.load_file_to_memory(save_file_name, "ram");
	}
    } catch (const filesystem_error& e) {
        cout << e.what();
	return 0;
    }

    state.a = 0x11; state.f = 0xb0; state.b = 0x00; state.c = 0x13;
    state.d = 0x00; state.e = 0xd8; state.h = 0x01; state.l = 0x4d;

    vector<pair<uint16_t, uint8_t>> memory_values = {
        {0xff00, 0xff}, {0xff05, 0x00}, {0xff06, 0x00}, {0xff07, 0x00},
	{0xff40, 0x91}, {0xff42, 0x00}, {0xff43, 0x00}, {0xff45, 0x00},
	{0xff47, 0xfc}, {0xff48, 0xff}, {0xff49, 0xff}, {0xff4a, 0x00},
	{0xff4b, 0x00}, {0xffff, 0x00}
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
    uint16_t save_counter = 0;
    while (!quit) {
	current_time_ms = SDL_GetTicks();
	cycles_to_catch_up += (current_time_ms - last_time_ms) * 1048;
	last_time_ms = current_time_ms;
	if (cycles_to_catch_up > 20000) {
	    cycles_to_catch_up = 20000;
	}
	handle_events(state);
	if ((state.read_memory(0xff00) & 0xf) != 0xf || state.read_memory(0xff0f) & 0x10) {state.stop_mode = false;}

	while (!quit && cycles_to_catch_up > 20 && !state.stop_mode) {
            uint8_t cycles_executed = 1;
            if (!state.halt_mode) {
 	        cycles_executed = execute_op(state) / 4;
	    }

	    uint8_t speed = state.double_speed ? 2 : 1;
 	    cycles_to_catch_up -= cycles_executed * speed;
	    draw_line_counter += cycles_executed;
	    timer_counter += cycles_executed * speed;
	    divider_counter += cycles_executed * speed;
	    event_counter += cycles_executed;
	    audio_counter += cycles_executed;
 	    if (cycles_to_catch_up < 20) {break;}

	    if (event_counter >= 100) {
	        handle_events(state);
		event_counter -= 100;
	    }

	    uint8_t speed_reg = state.read_memory(0xff4d);
	    state.write_memory(0xff4d, speed_reg | (state.double_speed ? 0x80 : 0x0));

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
	        save_counter++;
                if (state.save_pending && save_counter >= 20) {
		    save_counter = 0;
		    state.dump_memory_to_file(save_file_name, "ram");
		    state.save_pending = false;
		}

		if (state.read_memory(0xff44) <= 143 && !(state.read_memory(0xff55) & 0x80)) {
		    state.run_hdma();
		}

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
	if (state.cgb && state.stop_mode && state.read_memory(0xff4d) & 1) {
	    state.double_speed = !state.double_speed;
	    state.write_memory(0xff4d, state.double_speed ? 0x80 : 0x0);
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
