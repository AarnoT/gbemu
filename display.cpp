#include "display.h"
#include "state.h"

#include <cstdint>
#include <iostream>
#include <SDL2/SDL.h>

using std::uint8_t;
using std::uint16_t;
using std::uint32_t;

void draw_display_buffer(State& state, SDL_Surface* display_buffer)
{
    for (uint32_t n = 0; n < 1024; n++) {
	draw_background_tile(state, display_buffer, n);
    }
}

void draw_display_line(State& state, SDL_Surface* display_buffer, SDL_Surface* display_surface)
{
    if (state.read_memory(0xff44) < 144) {
        uint8_t display_row = state.read_memory(0xff44);
        uint8_t scroll_y = state.read_memory(0xff42);
        uint8_t scroll_x = state.read_memory(0xff43);
        uint32_t* buffer_pixels = (uint32_t*) display_buffer->pixels;
        uint32_t* display_pixels = (uint32_t*) display_surface->pixels;

        for (uint8_t display_column = 0; display_column < 160; display_column++) {
	    display_pixels[display_row*160 + display_column] =
	        buffer_pixels[(display_row + scroll_y) % 256 * 256 + (display_column + scroll_x) % 256];
        }
    }

    state.write_memory(0xff44, (state.read_memory(0xff44) + 1) % 154);
}

void draw_background_tile(State& state, SDL_Surface* surface, uint32_t tile_num)
{
    uint32_t tile_x = tile_num % 32, tile_y = (tile_num - tile_num % 32) / 32;
    uint32_t* pixels = (uint32_t*) surface->pixels;
    uint16_t tile_pointer = get_tile_pointer(state, tile_num);
    if (tile_pointer != 0x8200) std::cout << std::hex << tile_pointer << "\n";
    for (int i = 0; i < 16; i += 2) {
        uint8_t value1 = state.read_memory(tile_pointer + i);
        uint8_t value2 = state.read_memory(tile_pointer + i + 1);
        for (int j = 7; j >= 0; j --) {
            uint8_t shade = (value1 & (1 << j)) >> (j - 1);
	    shade |= (value2 & (1 << j)) >> j;
            shade = 255 - shade * 85;
            pixels[tile_y * 2048 + i * 128 + tile_x * 8 + 7 - j] = SDL_MapRGB(surface->format, shade, shade, shade);
	}
    }
}

uint16_t get_tile_pointer(State& state, uint32_t tile_num)
{
    uint8_t lcdc = state.read_memory(0xff40);
    uint8_t bg_code_area = (lcdc & 0x8) >> 3;
    uint8_t bg_data_area = (lcdc & 0x10) >> 4;
    uint8_t tile_code = 0;

    if (bg_code_area == 0) {
	tile_code = state.read_memory(0x9800 + tile_num);
    } else {
	tile_code = state.read_memory(0x9c00 + tile_num);
    }

    if (bg_data_area == 1) {
	return 0x8000 + tile_code * 0x10;
    } else {
	if (tile_code >= 0x80) {
            return 0x8800 + (tile_code - 0x80) * 0x10;
	} else {
	    return 0x9000 + tile_code * 0x10;
	}
    }
}
