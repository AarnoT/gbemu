#include "display.h"
#include "state.h"

#include <algorithm>
#include <cstdint>
#include <iostream>
#include <tuple>
#include <vector>

#include <SDL2/SDL.h>

using std::int16_t;
using std::uint8_t;
using std::uint16_t;
using std::uint32_t;
using std::copy;
using std::fill_n;
using std::get;
using std::sort;
using std::tuple;
using std::make_tuple;
using std::vector;

void draw_display_line(State& state, SDL_Surface* display_surface)
{
    if (state.read_memory(0xff44) < 144) {
        draw_line_background(state, display_surface);
        draw_line_window(state, display_surface);
        draw_line_sprites(state, display_surface);
    }
}

void draw_line_background(State& state, SDL_Surface* display_surface)
{
    uint8_t display_row = state.read_memory(0xff44);
    uint8_t scroll_y = state.read_memory(0xff42);
    uint8_t scroll_x = state.read_memory(0xff43);
    uint8_t lcdc = state.read_memory(0xff40);
    uint32_t* display_pixels = (uint32_t*) display_surface->pixels;

    if ((lcdc & 0x1) != 0) {
        uint8_t bgp = state.read_memory(0xff47);
        uint32_t palette[4];
        for (uint8_t i = 0; i < 4; i++) {
            uint8_t value = 255 - 85 * ((bgp & (3 << (i * 2))) >> (i * 2));
            palette[i] = SDL_MapRGB(display_surface->format, value, value, value);
        }

        uint32_t palette_cgb[4 * 8];
        for (uint8_t i = 0; i < 8; i++) {
	    for (uint8_t j = 0; j < 4; j++) {
	        uint16_t value = (
		    (state.bg_palettes[i * 8 + j * 2 + 1] << 8) | state.bg_palettes[i * 8 + j * 2]);
		uint8_t r = (value & 0x1f) * 8;
		uint8_t g = ((value & (0x1f << 5)) >> 5) * 8;
		uint8_t b = (((value & (0x1f << 10)) >> 10)) * 8;
		palette_cgb[i * 4 + j] = SDL_MapRGB(display_surface->format, r, g, b);
	    }
	}

        vector<uint16_t> tiles(0);
        uint8_t first_tile_x = scroll_x / 8;
        uint8_t last_tile_x = (scroll_x + 159) / 8;
        for (uint16_t x = first_tile_x; x <= last_tile_x; x++) {
            uint32_t tile_num = (display_row + scroll_y) % 256 / 8 * 32 + x % 32;
            tiles.push_back((get_tile_pointer(state, tile_num, false) - 0x8000) * 4);
        }

        uint8_t x_offset = scroll_x % 8;
        uint8_t y_offset = (display_row + scroll_y) % 8;

        for (uint32_t i = 0; i < tiles.size(); i++) {
            for (uint8_t pixel = 0; pixel < 8; pixel++) {
                int16_t x_pos = i * 8 + pixel - x_offset;
                if (x_pos >= 0 && x_pos < 160) {
		    uint32_t x = first_tile_x + i;
                    uint32_t tile_num = (display_row + scroll_y) % 256 / 8 * 32 + x % 32;
                    uint8_t code_area = (lcdc & 0x8) >> 3;
		    uint8_t attrs = state.read_vram_bank((code_area ? 0x1c00 : 0x1800) + tile_num);
		    uint8_t tile_offset = pixel;
	            if (state.cgb && (attrs & 0x20)) {
		        tile_offset = 7 - pixel;
	            }
		    uint8_t tile_y_offset = y_offset;
		    if (state.cgb && (attrs & 0x40)) {
			tile_y_offset = 7 - tile_y_offset;
		    }
		    uint8_t* tile_data = ((attrs & 0x8) && state.cgb) ? state.tile_data2 : state.tile_data;
		    uint32_t color = 0;
		    uint32_t shade = (tile_data[tiles[i] + tile_y_offset * 8 + tile_offset]);
                    if (shade == 1) {
		        shade = 2;
		    } else if (shade == 2) {
		        shade = 1;
		    }
		    if (state.cgb) {
                        color = palette_cgb[shade + 4 * (attrs & 0x7)];
		    } else {
                        color = palette[shade];
		    }
                    display_pixels[display_row * 160 + x_pos] = color; 
                }
            }
        }
    } else {
        fill_n(display_pixels + 160 * display_row, 160, SDL_MapRGB(display_surface->format, 0xff, 0xff, 0xff));
    }
}

void draw_line_window(State& state, SDL_Surface* display_surface)
{
    uint8_t display_row = state.read_memory(0xff44);
    uint8_t window_y = state.read_memory(0xff4a);
    uint8_t window_x = state.read_memory(0xff4b);
    uint8_t lcdc = state.read_memory(0xff40);
    uint32_t* display_pixels = (uint32_t*) display_surface->pixels;

    if ((lcdc & 0x20) != 0 && window_x <= 166 && window_y <= display_row) {
        uint8_t bgp = state.read_memory(0xff47);
        uint32_t palette[4];
        for (uint8_t i = 0; i < 4; i++) {
            uint8_t value = 255 - 85 * ((bgp & (3 << (i * 2))) >> (i * 2));
            palette[i] = SDL_MapRGB(display_surface->format, value, value, value);
        }

        uint32_t palette_cgb[4 * 8];
        for (uint8_t i = 0; i < 8; i++) {
	    for (uint8_t j = 0; j < 4; j++) {
	        uint16_t value = (
		    (state.bg_palettes[i * 8 + j * 2 + 1] << 8) | state.bg_palettes[i * 8 + j * 2]);
		uint8_t r = (value & 0x1f) * 8;
		uint8_t g = ((value & (0x1f << 5)) >> 5) * 8;
		uint8_t b = (((value & (0x1f << 10)) >> 10)) * 8;
		palette_cgb[i * 4 + j] = SDL_MapRGB(display_surface->format, r, g, b);
	    }
	}

        if (window_x <= 7) {
	    window_x = 0;
        } else {
	    window_x -= 7;
        }

        if ((lcdc & 0x1) != 0) {
	    vector<uint16_t> tiles(0);
	    uint8_t last_tile_x = (159 - window_x) / 8;
	    for (uint16_t x = 0; x <= last_tile_x; x++) {
	        uint32_t tile_num = (display_row - window_y) / 8 * 32 + x;
	        tiles.push_back((get_tile_pointer(state, tile_num, true) - 0x8000) * 4);
	    }

	    for (uint32_t i = 0; i < tiles.size(); i++) {
	        for (uint8_t pixel = 0; pixel < 8; pixel++) {
		    if (i * 8 + pixel + window_x >= 160) {
		        break;
		    }
	            uint32_t tile_num = (display_row - window_y) / 8 * 32 + i;
                    uint8_t code_area = (lcdc & 0x40) >> 6;
		    uint8_t attrs = state.read_vram_bank((code_area ? 0x1c00 : 0x1800) + tile_num);
		    uint8_t* tile_data = ((attrs & 0x8) && state.cgb) ? state.tile_data2 : state.tile_data;
		    uint32_t color = 0;
		    uint8_t tile_offset = pixel;
	            if (state.cgb && (attrs & 0x20)) {
		        tile_offset = 7 - pixel;
	            }
		    uint8_t tile_y_offset = (display_row - window_y) % 8;
		    if (state.cgb && (attrs & 0x40)) {
			tile_y_offset = 7 - tile_y_offset;
		    }
		    uint32_t shade = tile_data[tiles[i] + tile_y_offset * 8 + tile_offset];
                    if (shade == 1) {
		        shade = 2;
		    } else if (shade == 2) {
		        shade = 1;
		    }
		    if (state.cgb) {
                        color = palette_cgb[shade + 4 * (attrs & 0x7)];
		    } else {
                        color = palette[shade];
		    }
		    display_pixels[display_row * 160 + i * 8 + pixel + window_x] = color;
	        }
	    }
        } else {
	    fill_n(display_pixels + 160 * display_row, 160, SDL_MapRGB(display_surface->format, 0xff, 0xff, 0xff));
        }
    }
}

void draw_line_sprites(State& state, SDL_Surface* display_surface)
{
    uint8_t display_row = state.read_memory(0xff44);
    uint8_t lcdc = state.read_memory(0xff40);
    uint32_t* display_pixels = (uint32_t*) display_surface->pixels;

    if ((lcdc & 0x2) != 0) {
        uint8_t obp0 = state.read_memory(0xff48);
        uint8_t obp1 = state.read_memory(0xff49);
        uint32_t palette0[4];
        uint32_t palette1[4];
        for (uint8_t i = 0; i < 4; i++) {
	    uint8_t value = 255 - 85 * ((obp0 & (3 << (i * 2))) >> (i * 2));
	    palette0[i] = SDL_MapRGB(display_surface->format, value, value, value);
        }
        for (uint8_t i = 0; i < 4; i++) {
	    uint8_t value = 255 - 85 * ((obp1 & (3 << (i * 2))) >> (i * 2));
	    palette1[i] = SDL_MapRGB(display_surface->format, value, value, value);
        }

        uint32_t palette_cgb[4 * 8];
        for (uint8_t i = 0; i < 8; i++) {
	    for (uint8_t j = 0; j < 4; j++) {
	        uint16_t value = (
		    (state.obj_palettes[i * 8 + j * 2 + 1] << 8) | state.obj_palettes[i * 8 + j * 2]);
		uint8_t r = (value & 0x1f) * 8;
		uint8_t g = ((value & (0x1f << 5)) >> 5) * 8;
		uint8_t b = (((value & (0x1f << 10)) >> 10)) * 8;
		palette_cgb[i * 4 + j] = SDL_MapRGB(display_surface->format, r, g, b);
	    }
	}

	bool same_tiles = true;
	for (uint8_t i = 0; i < 40; i++) {
            uint8_t value = state.read_memory(0xfe02 + i * 4);
	    if (state.prev_oam_tile_ids[i] != value) {
                state.prev_oam_tile_ids[i] = value;
		same_tiles = false;
	    }
	}

	if (!same_tiles) {
            vector<tuple<uint8_t, uint8_t, uint8_t>> tmp_sprites(40);
	    for (uint8_t i = 0; i < 40; i++) {
		uint8_t sprite_x = state.read_memory(0xfe01 + i * 4);
		uint8_t sprite_tile = state.read_memory(0xfe02 + i * 4);
                tmp_sprites[i] = make_tuple(sprite_x, sprite_tile, i);
	    }
	    sort(tmp_sprites.begin(), tmp_sprites.end());
            for (uint8_t i = 0; i < 40; i++) {
	        state.sorted_sprites[39 - i] = get<2>(tmp_sprites[i]);
            }
	}


        uint8_t bgp = state.read_memory(0xff47);
        uint32_t bg_palette[4];
        for (uint8_t i = 0; i < 4; i++) {
            uint8_t value = 255 - 85 * ((bgp & (3 << (i * 2))) >> (i * 2));
            bg_palette[i] = SDL_MapRGB(display_surface->format, value, value, value);
        }

        uint32_t bg_palette_cgb[4 * 8];
        for (uint8_t i = 0; i < 8; i++) {
	    for (uint8_t j = 0; j < 4; j++) {
	        uint16_t value = (
		    (state.bg_palettes[i * 8 + j * 2 + 1] << 8) | state.bg_palettes[i * 8 + j * 2]);
		uint8_t r = (value & 0x1f) * 8;
		uint8_t g = ((value & (0x1f << 5)) >> 5) * 8;
		uint8_t b = (((value & (0x1f << 10)) >> 10)) * 8;
		bg_palette_cgb[i * 4 + j] = SDL_MapRGB(display_surface->format, r, g, b);
	    }
	}

        uint8_t sprite_height = (lcdc & 0x4) ? 16 : 8;
        uint8_t sprite_counter = 0;
        for (uint8_t i = 0; i < 40; i++) {
	    uint8_t sprite_id = state.sorted_sprites[i];
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

	    uint8_t sprite_row = display_row - sprite_y;
	    if (sprite_attrs & 0x40) {
	        sprite_row = sprite_height - sprite_row - 1;
	    }

	    uint8_t x_offset1 = (sprite_x < 0) ? -sprite_x : 0;
	    uint8_t x_offset2 = sprite_x + 7 - 159;
	    if (sprite_x + 7 < 159) {x_offset2 = 0;}

	    uint32_t tile_index = 64 * tile_id;
	    for (uint8_t i = x_offset1; i < 8 - x_offset2; i++) {
	        uint8_t pixel = i;
	        if (sprite_attrs & 0x20) {
		    pixel = 7 - pixel;
	        }

		uint32_t pixel_index = display_row * 160 + sprite_x + i;
		uint8_t* tile_data = ((sprite_attrs & 0x8) && state.cgb) ? state.tile_data2 : state.tile_data;
	        uint32_t shade = tile_data[tile_index + sprite_row * 8 + pixel];
                if (shade == 1) {
		    shade = 2;
		} else if (shade == 2) {
		    shade = 1;
		}
		if (state.cgb && shade != 0 && (sprite_attrs & 0x80) == 0) {
                    shade = palette_cgb[shade + 4 * (sprite_attrs & 0x7)];
		    display_pixels[pixel_index] = shade;
		} else if (shade != 0 && ((sprite_attrs & 0x80) == 0 || display_pixels[pixel_index] == bg_palette[0])) {
		    shade = (sprite_attrs & 0x10) ? palette1[shade] : palette0[shade];
		    display_pixels[pixel_index] = shade;
	        }
	    }

	    if (sprite_counter >= 10) {break;}
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
