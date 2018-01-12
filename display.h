#pragma once

#include "state.h"

#include <cstdint>
#include <SDL2/SDL.h>

void draw_display_buffer(State& state, SDL_Surface* display_buffer, bool window);
void draw_display_line(State& state, SDL_Surface* display_buffer, SDL_Surface* display_surface, SDL_Surface* window_surface);
void draw_background_tile(State& state, SDL_Surface* surface, std::uint32_t tile_num, bool window);
std::uint16_t get_tile_pointer(State& state, std::uint32_t tile_num, bool window);
