#include "state.h"
#include "instruction.h"

#include <algorithm>
#include <cstdint>
#include <iostream>

using std::cout;
using std::copy;
using std::hex;
using std::uint8_t;
using std::uint16_t;
using std::uint32_t;

State::State(uint8_t* buffer, uint32_t size) : memory(new uint8_t[0x60000]{0})
{
    copy(buffer, buffer + size, this->memory);
}

State::~State()
{
    delete this->memory;
}

uint8_t State::read_memory(uint16_t addr)
{
    if (true) {
	return this->memory[addr];
    } else {
	cout << "[WARNING]: Invalid memory read at " << hex << addr << ".\n";
    }
    return 0;
}

void State::write_memory(uint16_t addr, uint8_t value)
{
    if (addr >= 0x8000 && addr < 0xffff) {
        this->memory[addr] = value;
    } else {
        cout << "[WARNING]: Invalid memory write at " << hex << addr << ".\n";
    }
}
