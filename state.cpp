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

State::State() : memory(new uint8_t[0x60000]{0}) {}

State::~State()
{
    delete this->memory;
}

void State::dump_memory_to_file(string filename)
{
    ofstream output_file(filename, ofstream::binary);
    copy(this->memory, this->memory + 0x10000,
         ostreambuf_iterator<char>(output_file));
}

void State::load_file_to_memory(string filename)
{
    ifstream rom_file(filename, ifstream::binary);
    copy(istreambuf_iterator<char>(rom_file),
         istreambuf_iterator<char>(),
	 this->memory);
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
    if (true) {
        this->memory[addr] = value;
    } else {
        cout << "[WARNING]: Invalid memory write at " << hex << addr << ".\n";
    }
}
