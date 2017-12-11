#include "state.h"
#include "instruction.h"

#include <cstdint>
#include <iostream>

using std::cout;
using std::hex;
using std::uint8_t;
using std::uint16_t;

State::State() : memory(new uint8_t[0x60000]) {}

State::~State() {
    delete this->memory;
}

uint8_t State::read_memory(uint16_t addr, uint8_t value) {
    if (addr >= 0 && addr <= 0xffff) {
	return this->memory[addr];
    } else {
	cout << "[WARNING]: Invalid memory read at " << hex << addr << ".\n";
    }
}

void State::write_memory(uint16_t addr, uint8_t value) {
    if (addr >= 0x8000 && addr < 0xffff) {
        this->memory[addr] = value;
    } else {
        cout << "[WARNING]: Invalid memory write at " << hex << addr << ".\n";
    }
}
