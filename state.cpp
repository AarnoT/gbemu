#include "state.h"
#include "instruction.h"

#include <algorithm>
#include <cstdint>
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
using std::uint8_t;
using std::uint16_t;
using std::uint32_t;

State::State() : memory(new uint8_t[0x10000]{0}) {}

State::~State()
{
    delete this->memory;
    if (this->ram != nullptr) {delete this->ram;}
    if (this->rom != nullptr) {delete this->rom;}
}

void State::dump_memory_to_file(string filename)
{
    ofstream output_file(filename, ofstream::binary);
    copy(this->memory, this->memory + 0x10000,
         ostreambuf_iterator<char>(output_file));
}

void State::load_file_to_memory(string filename)
{
    ifstream memory_state(filename, ifstream::binary);
    copy(istreambuf_iterator<char>(memory_state),
         istreambuf_iterator<char>(),
	 this->memory);
}

void State::load_file_to_rom(string filename)
{
    ifstream rom_file(filename, ifstream::binary);
    char tmp_buffer[0x150];
    rom_file.read(tmp_buffer, 0x150);
    rom_file.seekg(0);

    uint32_t rom_size = tmp_buffer[0x148];
    switch (rom_size) {
    case 0x52: rom_size = 0x120000; break;
    case 0x53: rom_size = 0x140000; break;
    case 0x54: rom_size = 0x180000; break;
    default: rom_size = 0x8000 << rom_size; break;
    }
    this->rom = new uint8_t[rom_size]{0};

    uint32_t ram_size = tmp_buffer[0x149];
    switch (ram_size) {
    case 0x00: ram_size = 0; break;
    case 0x01: ram_size = 0x800; break;
    case 0x02: ram_size = 0x2000; break;
    case 0x03: ram_size = 0x8000; break;
    case 0x04: ram_size = 0x20000; break;
    case 0x05: ram_size = 0x10000; break;
    }
    if (ram_size != 0) {
        this->ram = new uint8_t[ram_size]{0};
    }

    copy(istreambuf_iterator<char>(rom_file),
         istreambuf_iterator<char>(),
	 this->rom);
}

uint8_t State::read_memory(uint16_t addr)
{

    uint8_t mbc = this->rom[0x147];
    if (mbc >= 1 && mbc <= 3) {mbc = 1;}
    if (mbc >= 0x19 && mbc <= 0x1e) {mbc = 5;}

    if (addr <= 0x7fff || (addr >= 0xa000 && addr <= 0xbfff)) {
        if (addr <= 0x3fff) {
            return this->rom[addr];
        } else if (mbc == 0 && addr <= 0x7fff) {
            return this->rom[addr];
	} else if (mbc == 1) {
            return this->read_mbc1(addr);
	} else if (mbc == 5) {
            return this->read_mbc5(addr);
	}
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

void State::write_memory(uint16_t addr, uint8_t value)
{
    uint8_t mbc = this->rom[0x147];
    if (mbc >= 1 && mbc <= 3) {mbc = 1;}
    if (mbc >= 0x19 && mbc <= 0x1e) {mbc = 5;}

    if ((addr <= 0x7fff || (addr >= 0xa000 && addr <= 0xbfff)) && mbc == 1) {
        this->write_mbc1(addr, value);
    } else if ((addr <= 0x5fff || (addr >= 0xa000 && addr <= 0xbfff)) && mbc == 5) {
        this->write_mbc5(addr, value);
    } else if (addr == 0xff46 && value >= 0x80 && value <= 0xdf) {
        copy(this->memory + (value << 8), this->memory + (value << 8) + 0x100, this->memory + 0xfe00);
    } else if ((addr >= 0xe000 && addr <= 0xfdff) || (addr >= 0xfea0 && addr <= 0xfeff)) {
        cout << "[WARNING]: Invalid memory write from " << hex << this->pc << ".\n";
    } else {
        this->memory[addr] = value;
    }
}

void State::write_mbc1(uint16_t addr, uint8_t value)
{
    if (addr <= 0x1fff) {
        this->ram_enabled = (value & 0xa) == 0xa;
    } else if (addr >= 0x2000 && addr <= 0x3fff) {
        value &= 0x1f;
	if ((value & 0xf) == 0) {value |= 1;}
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
    } else if (this->ram != nullptr && addr >= 0xa000 && addr <= 0xbfff) {
        uint8_t effective_ram_bank = this->ram_bank_mode ? this->ram_bank : 0;
        this->ram[0x2000 * effective_ram_bank + addr - 0xa000] = value;
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
    } else if (this->ram != nullptr && addr >= 0xa000 && addr <= 0xbfff) {
        this->ram[0x2000 * this->ram_bank + addr - 0xa000] = value;
    }
}
