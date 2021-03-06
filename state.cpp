#include "state.h"
#include "instruction.h"

#include <algorithm>
#include <cstdint>
#include <ctime>
#include <fstream>
#include <iostream>
#include <iterator>
#include <string>

using std::cout;
using std::copy;
using std::hex;
using std::ifstream;
using std::istreambuf_iterator;
using std::ofstream;
using std::ostreambuf_iterator;
using std::string;
using std::time;
using std::time_t;
using std::int8_t;
using std::uint8_t;
using std::uint16_t;
using std::uint32_t;

State::State() : tile_data(new uint8_t[0x8000]{0}),
                 tile_data2(new uint8_t[0x8000]{0}),
                 memory(new uint8_t[0x10000]{0}), 
                 wram_banks(new uint8_t[0x8000]{0}),
                 vram_banks(new uint8_t[0x2000]{0}) {}

State::~State()
{
    delete this->memory;
    delete this->tile_data;
    delete this->tile_data2;
    delete this->wram_banks;
    delete this->vram_banks;
    if (this->ram != nullptr) {delete this->ram;}
    if (this->rom != nullptr) {delete this->rom;}
}

void State::dump_memory_to_file(string filename, string memory="work ram")
{
    uint8_t* mem = nullptr;
    uint32_t size = 0;
    if (memory == "work ram") {
	size = 0x10000;
	mem = this->memory;
    } else if (memory == "ram" && ram != nullptr) {
        size = this->ram_size;
       	mem = this->ram;
    } else {
	return;
    }
    ofstream output_file(filename, ofstream::binary);
    copy(mem, mem + size,
         ostreambuf_iterator<char>(output_file));
}

bool State::load_file_to_memory(string filename, string memory="work ram")
{
    uint8_t* mem = nullptr;
    if (memory == "work ram") {
        mem = this->memory;
    } else if (memory == "ram" && ram != nullptr) {
	mem = this->ram;
    } else {
	return false;
    }
    ifstream memory_state(filename, ifstream::binary);
    copy(istreambuf_iterator<char>(memory_state),
         istreambuf_iterator<char>(),
         mem);

    return static_cast<bool>(memory_state);
}

uint8_t State::read_vram_bank(uint16_t addr)
{
    if (addr >= 0x2000) {
        return 0xff;
    }
    return this->vram_banks[addr];
}

bool State::load_file_to_rom(string filename)
{
    ifstream rom_file(filename, ifstream::binary);
    char tmp_buffer[0x150];
    rom_file.read(tmp_buffer, 0x150);
    rom_file.seekg(0);

    this->cgb = ((uint8_t) tmp_buffer[0x143]) == 0x80 || ((uint8_t) tmp_buffer[0x143]) == 0xc0;

    uint32_t rom_size = tmp_buffer[0x148];
    switch (rom_size) {
    case 0x52: rom_banks = 72; rom_size = 0x120000; break;
    case 0x53: rom_banks = 80; rom_size = 0x140000; break;
    case 0x54: rom_banks = 96; rom_size = 0x180000; break;
    default: rom_banks = 2 << rom_size; rom_size = 0x8000 << rom_size; break;
    }
    this->rom = new uint8_t[rom_size]{0};

    ram_size = tmp_buffer[0x149];
    switch (ram_size) {
    case 0x00: ram_banks = 0; ram_size = 0; break;
    case 0x01: ram_banks = 1; ram_size = 0x800; break;
    case 0x02: ram_banks = 1; ram_size = 0x2000; break;
    case 0x03: ram_banks = 4; ram_size = 0x8000; break;
    case 0x04: ram_banks = 16; ram_size = 0x20000; break;
    case 0x05: ram_banks = 8; ram_size = 0x10000; break;
    }
    uint8_t mbc = tmp_buffer[0x147];
    if (mbc == 5 || mbc == 6) {ram_size = 0x200;}

    if (mbc >= 1 && mbc <= 3) {rom_bank = 1;}
    if (mbc == 5 || mbc == 6) {rom_bank = 1;}
    if (mbc >= 0xf && mbc <= 0x13) {rom_bank = 1;}

    if (ram_size != 0) {
        this->ram = new uint8_t[ram_size]{0};
    }

    copy(istreambuf_iterator<char>(rom_file),
         istreambuf_iterator<char>(),
	 this->rom);

    return static_cast<bool>(rom_file);
}

uint8_t State::read_memory(uint16_t addr)
{

    uint8_t mbc = this->rom[0x147];
    if (mbc >= 1 && mbc <= 3) {mbc = 1;}
    if (mbc == 5 || mbc == 6) {mbc = 2;}
    if (mbc >= 0xf && mbc <= 0x13) {mbc = 3;}
    if (mbc >= 0x19 && mbc <= 0x1e) {mbc = 5;}

    if (addr <= 0x7fff || (addr >= 0xa000 && addr <= 0xbfff)) {
        if (addr <= 0x3fff) {
            return this->rom[addr];
        } else if (mbc == 0 && addr <= 0x7fff) {
            return this->rom[addr];
	} else if (mbc == 1) {
            return this->read_mbc1(addr);
	} else if (mbc == 2 && addr <= 0xa1ff) {
            return this->read_mbc2(addr);
	} else if (mbc == 3) {
            return this->read_mbc3(addr);
	} else if (mbc == 5) {
            return this->read_mbc5(addr);
	}
    } else if (this->cgb && addr >= 0xd000 && addr <= 0xdfff) {
        return this->wram_banks[wram_bank * 0x1000 + addr - 0xd000];
    } else if (this->cgb && addr == 0xff4f) {
        return 0xfe | this->vram_bank;
    } else if (this->cgb && addr == 0xff4d) {
        return (this->double_speed ? 0x80 : 0) | (this->prepare_double_speed ? 1 : 0);
    } else if (this->cgb && addr == 0xff69) {
        return this->bg_palettes[this->memory[0xff68] & 0x3f];
    } else if (this->cgb && addr == 0xff6b) {
        return this->obj_palettes[this->memory[0xff6a] & 0x3f];
    } else if (this->cgb && addr == 0xff70) {
        return this->wram_bank;
    } else if (this->cgb && this->vram_bank == 1 && addr >= 0x8000 && addr <= 0x9fff) {
        return this->vram_banks[addr - 0x8000];
    } else {
        if ((addr >= 0xe000 && addr <= 0xfdff) || (addr >= 0xfea0 && addr <= 0xfeff)) {
	    cout << "[WARNING]: Invalid memory read from " << hex << this->pc << ".\n";
        }
        return this->memory[addr];
    }
    return 0;
}

uint8_t State::read_mbc1(uint16_t addr)
{
    if (addr >= 0x4000 && addr <= 0x7fff) {
        uint8_t effective_rom_bank = this->ram_bank_mode ? (this->rom_bank & 0x1f) : this->rom_bank;
        return this->rom[0x4000 * effective_rom_bank + addr - 0x4000];
    } else {
        if (!this->ram_enabled || this->ram == nullptr) {
            return 0xff;
	} else {
            uint8_t effective_ram_bank = this->ram_bank_mode ? this->ram_bank : 0;
            return this->ram[0x2000 * effective_ram_bank + addr - 0xa000];
	}
    }
}

uint8_t State::read_mbc2(uint16_t addr)
{
    if (addr >= 0x4000 && addr <= 0x7fff) {
        return this->rom[0x4000 * this->rom_bank + addr - 0x4000];
    } else {
        if (!this->ram_enabled) {
            return 0xff;
	} else {
            return this->ram[addr - 0xa000] & 0xf;
	}
    }
}

uint8_t State::read_mbc3(uint16_t addr)
{
    if (addr >= 0x4000 && addr <= 0x7fff) {
        return this->rom[0x4000 * this->rom_bank + addr - 0x4000];
    } else {
        if (!this->ram_enabled) {
            return 0xff;
        } else if (this->ram_bank >= 0x8 && this->ram_bank <= 0xc) {
            switch (this->ram_bank) {
            case 0x8: return this->rtc_seconds;
            case 0x9: return this->rtc_minutes;
            case 0xa: return this->rtc_hours;
            case 0xb: return this->rtc_days & 0xff;
            case 0xc:
                {
                    uint8_t days_bit8 = (rtc_days & 0x100) >> 8;
		    return this->rtc_flags | days_bit8;
		}
	    }
	} else if (this->ram == nullptr) {
            return 0xff;
	} else if (this->ram_bank <= 3) {
            return this->ram[0x2000 * this->ram_bank + addr - 0xa000];
	}
    }
    return 0xff;
}

uint8_t State::read_mbc5(uint16_t addr)
{
    if (addr >= 0x4000 && addr <= 0x7fff) {
        return this->rom[0x4000 * this->rom_bank + addr - 0x4000];
    } else {
        if (!this->ram_enabled || this->ram == nullptr) {
            return 0xff;
	} else {
            return this->ram[0x2000 * this->ram_bank + addr - 0xa000];
	}
    }
}

void State::run_hdma()
{
    if (this->hdma_len <= 0) {
        this->memory[0xff55] = 0xff;
	return;
    }

    uint32_t len = (this->hdma_len < 0x10) ? this->hdma_len : 0x10;
    copy(this->hdma_src, this->hdma_src + len, this->hdma_dest);
    this->hdma_len -= len;
    this->hdma_src += len;
    this->hdma_dest += len;

    uint16_t src = (this->memory[0xff51] << 8 | this->memory[0xff52]) & 0xfff0;
    uint16_t dest = (this->memory[0xff53] << 8 | this->memory[0xff54]) & 0x1ff0;
    src += len;
    dest += len;
    this->memory[0xff51] = (src & 0xfff0) >> 8;
    this->memory[0xff52] = (src & 0xfff0) && 0xff;
    this->memory[0xff53] = (dest & 0x1ff0) >> 8;
    this->memory[0xff54] = (dest & 0x1ff0) && 0xff;
    this->memory[0xff55] = (this->hdma_len <= 0) ? 0xff : (this->hdma_len / 0x10 - 1);
}

void State::write_memory(uint16_t addr, uint8_t value)
{
    uint8_t mbc = this->rom[0x147];
    if (mbc >= 1 && mbc <= 3) {mbc = 1;}
    if (mbc == 5 || mbc == 6) {mbc = 2;}
    if (mbc >= 0xf && mbc <= 0x13) {mbc = 3;}
    if (mbc >= 0x19 && mbc <= 0x1e) {mbc = 5;}

    uint16_t prev_rom_bank = rom_bank;
    uint16_t prev_ram_bank = ram_bank;

    if ((addr <= 0x7fff || (addr >= 0xa000 && addr <= 0xbfff)) && mbc == 1) {
        this->write_mbc1(addr, value);
    } else if ((addr <= 0x3fff || (addr >= 0xa000 && addr <= 0xa1ff)) && mbc == 2) {
        this->write_mbc2(addr, value);
    } else if ((addr <= 0x7fff || (addr >= 0xa000 && addr <= 0xbfff)) && mbc == 3) {
        this->write_mbc3(addr, value);
    } else if ((addr <= 0x5fff || (addr >= 0xa000 && addr <= 0xbfff)) && mbc == 5) {
        this->write_mbc5(addr, value);
    } else if (this->cgb && this->vram_bank == 1 && addr >= 0x8000 && addr <= 0x9fff) {
        this->vram_banks[addr - 0x8000] = value;
    } else if (this->cgb && addr >= 0xd000 && addr <= 0xdfff) {
        this->wram_banks[wram_bank * 0x1000 + addr - 0xd000] = value;
    } else if (addr == 0xff46 && value <= 0xf1) {
        uint8_t* src = this->memory + (value << 8);
        if (this->cgb && this->vram_bank == 1 && value >= 0x80 && value <= 0x9f) {
	    src = this->vram_banks + (value << 8) - 0x8000;
	} else if (this->ram_enabled && this->ram != nullptr && value >= 0xa0 && value <= 0xbf) {
	    uint8_t effective_ram_bank = this->ram_bank_mode ? this->ram_bank : 0;
	    src = this->ram + 0x2000 * effective_ram_bank + (value << 8) - 0xa000;
	}
	copy(src, src + 0xa0, this->memory + 0xfe00);
    } else if (this->cgb && addr == 0xff55 && (value & 0x7f) != 0) {
	if (!(value & 0x80) && !(this->memory[0xff55] & 0x80)) {
	    this->memory[0xff55] |= 0x80;
	    return;
	}

        uint16_t src = (this->memory[0xff51] << 8 | this->memory[0xff52]) & 0xfff0;
        uint16_t dest = (this->memory[0xff53] << 8 | this->memory[0xff54]) & 0x1ff0;
	uint16_t len = ((value & 0x7f) + 1) * 0x10;
	uint8_t* vram_ptr = (this->vram_bank == 1) ? this->vram_banks : (this->memory + 0x8000);
	uint8_t* mem_ptr = nullptr;

        if (dest + len >= 0x2000) {
	    len = 0x2000 - dest;
	}

	if (src <= 0x3ff0) {
	    if (src + len >= 0x4000) {
                len = 0x4000 - src;
	    }
	    mem_ptr = this->memory + src;
	} else if (src <= 0x7ff0) {
	    if (src + len >= 0x8000) {
                len = 0x8000 - src;
	    }
	    mem_ptr = this->rom + 0x4000 * this->rom_bank + src - 0x4000;
	} else if (this-ram_enabled && this->ram != nullptr && src >= 0xa000 && src <= 0xbff0) {
	    if (src + len >= 0xc000) {
                len = 0xc000 - src;
	    }
	    mem_ptr = this->ram + 0x2000 * this->ram_bank + src - 0xa000;
	} else if (src >= 0xc000 && src <= 0xcff0) {
	    if (src + len >= 0xd000) {
                len = 0xd000 - src;
	    }
	    mem_ptr = this->memory + src;
	} else if (src >= 0xd000 && src <= 0xdff0) {
	    if (src + len >= 0xe000) {
                len = 0xe000 - src;
	    }
	    mem_ptr = this->wram_banks + this->wram_bank * 0x1000 + src - 0xd000;
	}

	if (mem_ptr != nullptr && (value & 0x80)) {
	    this->hdma_len = len;
	    this->hdma_src = mem_ptr;
	    this->hdma_dest = vram_ptr + dest;
	    this->memory[0xff55] = len / 0x10 - 1;
	} else if (!(value & 0x80)) {
	    if (mem_ptr != nullptr) {
	        copy(mem_ptr, mem_ptr + len, vram_ptr + dest);
	    }
	    this->memory[0xff51] = ((src + len) & 0xfff0) >> 8;
	    this->memory[0xff52] = ((src + len) & 0xfff0) && 0xff;
	    this->memory[0xff53] = ((dest + len) & 0x1ff0) >> 8;
	    this->memory[0xff54] = ((dest + len) & 0x1ff0) && 0xff;
	    this->memory[0xff55] = 0xff;
	    this->prev_gdma_len = len / 0x10 - 1;
	}
    } else if (this->cgb && addr == 0xff4d) {
        this->prepare_double_speed = value & 0x1;
    } else if (this->cgb && addr == 0xff4f) {
        this->vram_bank = value & 1;
    } else if (this->cgb && addr == 0xff69) {
        uint8_t index = this->memory[0xff68] & 0x3f;
        this->bg_palettes[index] = value;
	if (this->memory[0xff68] & 0x80) {
	    this->memory[0xff68] += 1;
	    this->memory[0xff68] &= 0x80 | 0x3f;
	}
    } else if (this->cgb && addr == 0xff6b) {
        uint8_t index = this->memory[0xff6a] & 0x3f;
        this->obj_palettes[index] = value;
	if (this->memory[0xff6a] & 0x80) {
	    this->memory[0xff6a] += 1;
	    this->memory[0xff6a] &= 0x80 | 0x3f;
	}
    } else if (this->cgb && addr == 0xff70) {
        this->wram_bank = (value & 7) | 1;
    } else if ((addr >= 0xe000 && addr <= 0xfdff) || (addr >= 0xfea0 && addr <= 0xfeff)) {
        cout << "[WARNING]: Invalid memory write from " << hex << this->pc << ".\n";
    } else {
        this->memory[addr] = value;
    }

    if (rom_bank >= rom_banks) {
        rom_bank = prev_rom_bank;
    }
    if (ram_bank >= ram_banks) {
        ram_bank = prev_ram_bank;
    }
}

void State::write_mbc1(uint16_t addr, uint8_t value)
{
    if (addr <= 0x1fff) {
        this->ram_enabled = (value & 0xf) == 0xa;
    } else if (addr >= 0x2000 && addr <= 0x3fff) {
        value &= 0x1f;
	if ((value & 0x1f) == 0) {value |= 1;}
	this->rom_bank &= ~0x1f;
	this->rom_bank |= value;
    } else if (addr >= 0x4000 && addr <= 0x5fff) {
        if (this->ram_bank_mode) {
            this->ram_bank = value & 0x3;
	} else {
            value = (value & 0x3) << 5;
	    this->rom_bank &= ~0x60;
	    this->rom_bank |= value;
	}
    } else if (addr >= 0x6000 && addr <= 0x7fff) {
        this->ram_bank_mode = (value & 0x1) != 0;
    } else if (this->ram_enabled && this->ram != nullptr && addr >= 0xa000 && addr <= 0xbfff) {
        uint8_t effective_ram_bank = this->ram_bank_mode ? this->ram_bank : 0;
        this->ram[0x2000 * effective_ram_bank + addr - 0xa000] = value;
	save_pending = true;
    }
}

void State::write_mbc2(uint16_t addr, uint8_t value)
{
    if (addr <= 0x1fff && !(addr & 0x100)) {
        this->ram_enabled = value == 0xa;
    } else if (addr >= 0x2000 && addr <= 0x3fff && (addr & 0x100)) {
        value &= 0xf;
	if ((value & 0xf) == 0) {value |= 1;}
	this->rom_bank = value;
    } else if (this->ram_enabled && addr >= 0xa000 && addr <= 0xa1ff) {
        this->ram[addr - 0xa000] = value & 0xf;
	save_pending = true;
    }
}

void State::write_mbc3(uint16_t addr, uint8_t value)
{
    if (addr <= 0x1fff) {
        this->ram_enabled = value == 0xa;
    } else if (addr >= 0x2000 && addr <= 0x3fff) {
	this->rom_bank = value & 0x7f;
    } else if (addr >= 0x4000 && addr <= 0x5fff) {
        this->ram_bank = value;
    } else if (addr >= 0x6000 && addr <= 0x7fff) {
        if (this->prev_rtc_latch == 0 && value == 1) {
            time_t now = time(0);
	    this->rtc_seconds = now % 60;
	    this->rtc_minutes = (now / 60) % 60;
	    this->rtc_hours = (now / 3600) % 24;
	    uint16_t prev_rtc_days = this->rtc_days;
	    this->rtc_days = (now / 3600 / 24) % 512;
	    if (rtc_days < prev_rtc_days) {
                this->rtc_flags |= 0x80;
	    }
	}
	this->prev_rtc_latch = value;
    } else if (this->ram_enabled && addr >= 0xa000 && addr <= 0xbfff) {
        if (this->ram_bank >= 0x8 && this->ram_bank <= 0xc) {
            switch (this->ram_bank) {
            case 0x8: this->rtc_seconds = value; break;
            case 0x9: this->rtc_minutes = value; break;
            case 0xa: this->rtc_hours = value; break;
            case 0xb: this->rtc_days = (this->rtc_days & 0xff00) | value; break;
            case 0xc:
                {
		    this->rtc_flags = value;
		    this->rtc_days &= 0x00ff;
		    this->rtc_days |= (value & 0x1) << 8;
		}
                break;  
	    }
	} else if (this->ram != nullptr && this->ram_bank <= 3) {
            this->ram[0x2000 * this->ram_bank + addr - 0xa000] = value;
	    save_pending = true;
	}
    }
}

void State::write_mbc5(uint16_t addr, uint8_t value)
{
    if (addr <= 0x1fff) {
        this->ram_enabled = value == 0xa;
    } else if (addr >= 0x2000 && addr <= 0x2fff) {
        this->rom_bank &= 0xff00;
	this->rom_bank |= value;
    } else if (addr >= 0x3000 && addr <= 0x3fff) {
        this->rom_bank &= 0x00ff;
	this->rom_bank |= (value & 0x1) << 8;
    } else if (addr >= 0x4000 && addr <= 0x5fff) {
        this->ram_bank = value & 0x0f;
    } else if (this->ram_enabled && this->ram != nullptr && addr >= 0xa000 && addr <= 0xbfff) {
        this->ram[0x2000 * this->ram_bank + addr - 0xa000] = value;
	save_pending = true;
    }
}

void State::update_tile_data()
{
    for (uint32_t i = 0; i < 0x1000; i++) {
        uint8_t data1 = this->memory[0x8000 + i * 2];
        uint8_t data2 = this->memory[0x8000 + i * 2 + 1];

	for (int8_t n = 7; n >= 0; n--) {
	    uint8_t pixel = 0;
            pixel |= (data1 & (1 << n)) >> n << 1;
            pixel |= (data2 & (1 << n)) >> n;
	    this->tile_data[i * 8 + (7 - n)] = pixel;
	}
    }

    if (!this->cgb) {
        return;
    }

    for (uint32_t i = 0; i < 0x1000; i++) {
        uint8_t data1 = this->vram_banks[i * 2];
        uint8_t data2 = this->vram_banks[i * 2 + 1];

	for (int8_t n = 7; n >= 0; n--) {
	    uint8_t pixel = 0;
            pixel |= (data1 & (1 << n)) >> n << 1;
            pixel |= (data2 & (1 << n)) >> n;
	    this->tile_data2[i * 8 + (7 - n)] = pixel;
	}
    }
}

