#include "display.h"
#include "state.h"

#include <cstdint>

using std::uint8_t;
using std::uint16_t;
using std::uint32_t;

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
	if (tile_num < 0x80) {
            return 0x8800 + (tile_code - 0x80) * 0x10;
	} else {
	    return 0x9000 + tile_code * 0x10;
	}
    }
}
