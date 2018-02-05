#pragma once

#include "state.h"

#include <cstdint>
#include <SDL2/SDL.h>

void draw_display_line(State& state, SDL_Surface* display_surface);
void draw_line_background(State& state, SDL_Surface* display_surface);
void draw_line_window(State& state, SDL_Surface* display_surface);
void draw_line_sprites(State& state, SDL_Surface* display_surface);
std::uint16_t get_tile_pointer(State& state, std::uint32_t tile_num, bool window);
