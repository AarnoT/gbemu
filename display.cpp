#include "display.h"
#include "state.h"

#include <cstdint>
#include <iostream>
#include <vector>

#include <SDL2/SDL.h>

using std::uint8_t;
using std::uint16_t;
using std::uint32_t;
using std::vector;

void draw_display_buffer(State& state, SDL_Surface* display_buffer, bool window)
{
    for (uint32_t n = 0; n < 1024; n++) {
	draw_background_tile(state, display_buffer, n, window);
    }
}

void draw_display_line(State& state, SDL_Surface* display_buffer, SDL_Surface* display_surface, SDL_Surface* window_surface)
{
    if (state.read_memory(0xff44) < 144) {
        uint8_t display_row = state.read_memory(0xff44);
        uint8_t scroll_y = state.read_memory(0xff42);
        uint8_t scroll_x = state.read_memory(0xff43);
        uint8_t window_y = state.read_memory(0xff4a);
        uint8_t window_x = state.read_memory(0xff4b);
	uint8_t lcdc = state.read_memory(0xff40);
        uint32_t* buffer_pixels = (uint32_t*) display_buffer->pixels;
        uint32_t* display_pixels = (uint32_t*) display_surface->pixels;
        uint32_t* window_pixels = (uint32_t*) window_surface->pixels;

        for (uint8_t display_column = 0; display_column < 160; display_column++) {
            uint32_t value = SDL_MapRGB(display_surface->format, 0xff, 0xff, 0xff);
	    if ((lcdc & 0x1) != 0) {
                value = buffer_pixels[(display_row + scroll_y) % 256 * 256 + (display_column + scroll_x) % 256];
	    }
	    display_pixels[display_row*160 + display_column] = value;
        }
	if ((lcdc & 0x2) != 0) {
            uint8_t obp0 = state.read_memory(0xff48);
            uint8_t obp1 = state.read_memory(0xff49);
            uint8_t palette0[4];
            uint8_t palette1[4];
            for (uint8_t i = 0; i < 4; i++) {
                palette0[i] = 255 - 85 * ((obp0 & (3 << (i * 2))) >> (i * 2));
            }
            for (uint8_t i = 0; i < 4; i++) {
                palette1[i] = 255 - 85 * ((obp1 & (3 << (i * 2))) >> (i * 2));
            }

            vector<uint8_t> sprites(40);
	    for (uint8_t n = 0; n < 40; n++) {
		sprites[n] = n;
	    }

	    uint8_t sprite_height = (lcdc & 0x4) ? 16 : 8;
	    uint8_t sprite_counter = 0;
	    for (uint8_t sprite_id : sprites) {
		int16_t sprite_y = (int16_t) state.read_memory(0xfe00 + 4 * sprite_id) - 16;
		int16_t sprite_x = (int16_t) state.read_memory(0xfe01 + 4 * sprite_id) - 8;
		uint8_t tile_id = state.read_memory(0xfe02 + 4 * sprite_id);
		if (sprite_height == 16) {
                    tile_id &= 0xfe;
		}
		uint8_t sprite_attrs = state.read_memory(0xfe03 + 4 * sprite_id);

                if (sprite_y <= display_row && sprite_y + sprite_height - 1 >= display_row) {
                    sprite_counter++;
		} else {
                    continue;
		}

		uint16_t tile_pointer = 0x8000 + 16 * tile_id;
		uint8_t sprite_row = display_row - sprite_y;
		if (sprite_attrs & 0x40) {
                    sprite_row = sprite_height - sprite_row - 1;
		}

                uint8_t byte1 = state.read_memory(tile_pointer + sprite_row * 2);
                uint8_t byte2 = state.read_memory(tile_pointer + sprite_row * 2 + 1);
		uint8_t x_offset1 = (sprite_x < 0) ? -sprite_x : 0;
		uint8_t x_offset2 = sprite_x + 7 - 159;
		if (sprite_x + 7 < 159) {x_offset2 = 0;}

                for (int j = 7 - x_offset1; j >= x_offset2; j--) {
                    uint8_t x = j;
                    if (sprite_attrs & 0x20) {
                        x = 7 - j;
		    }
                    uint8_t shade = (byte1 & (1 << x));
		    if (x != 0) {
                        shade = shade >> (x - 1);
		    } else {
                        shade = shade << 1;
		    }
	            shade |= (byte2 & (1 << x)) >> x;
		    if (shade == 0) {continue;}
                    shade = (sprite_attrs & 0x10) ? palette1[shade] : palette0[shade];
                    display_pixels[display_row * 160 + sprite_x + (7 - j)]
		        = SDL_MapRGB(display_surface->format, shade, shade, shade);
	        }

                if (sprite_counter >= 10) {break;}
	    }
	}
	if ((lcdc & 0x20) != 0 && window_x <= 166 && window_y <= display_row) {
	    if (window_x <= 7) {
                window_x = 0;
	    } else {
                window_x -= 7;
	    }
            for (uint8_t x = window_x; x < 160; x++) {
                uint32_t value = SDL_MapRGB(display_surface->format, 0xff, 0xff, 0xff);
	        if ((lcdc & 0x1) != 0) {
                    value = window_pixels[(display_row - window_y) * 256 + x - window_x];
		}
                display_pixels[display_row*160 + x] = value;
	    }
	}
    }

    state.write_memory(0xff44, (state.read_memory(0xff44) + 1) % 154);
}

void draw_background_tile(State& state, SDL_Surface* surface, uint32_t tile_num, bool window)
{
    uint8_t bgp = state.read_memory(0xff47);
    uint32_t palette[4];
    for (uint8_t i = 0; i < 4; i++) {
        uint8_t value = 255 - 85 * ((bgp & (3 << (i * 2))) >> (i * 2));
	palette[i] = SDL_MapRGB(surface->format, value, value, value);
    }
    uint32_t tile_x = tile_num % 32, tile_y = (tile_num - tile_num % 32) / 32;
    uint32_t* pixels = (uint32_t*) surface->pixels;
    uint16_t tile_pointer = get_tile_pointer(state, tile_num, window);
    for (int i = 0; i < 16; i += 2) {
        uint8_t value1 = state.read_memory(tile_pointer + i);
        uint8_t value2 = state.read_memory(tile_pointer + i + 1);
        for (int j = 7; j >= 0; j --) {
            uint8_t shade = (value1 & (1 << j));
	    if (j != 0) {
                shade = shade >> (j - 1);
	    } else {
                shade = shade << 1;
	    }
	    shade |= (value2 & (1 << j)) >> j;
            pixels[tile_y * 2048 + i * 128 + tile_x * 8 + 7 - j] = palette[shade];
	}
    }
}

uint16_t get_tile_pointer(State& state, uint32_t tile_num, bool window)
{
    uint8_t lcdc = state.read_memory(0xff40);
    uint8_t code_area = 0;
    if (window) {
        code_area = (lcdc & 0x40) >> 6;
    } else {
        code_area = (lcdc & 0x8) >> 3;
    }
    uint8_t data_area = (lcdc & 0x10) >> 4;
    uint8_t tile_code = 0;

    if (code_area == 0) {
	tile_code = state.read_memory(0x9800 + tile_num);
    } else {
	tile_code = state.read_memory(0x9c00 + tile_num);
    }

    if (data_area == 1) {
	return 0x8000 + tile_code * 0x10;
    } else {
	if (tile_code >= 0x80) {
            return 0x8800 + (tile_code - 0x80) * 0x10;
	} else {
	    return 0x9000 + tile_code * 0x10;
	}
    }
}
