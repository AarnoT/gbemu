#include "emulator.h"
#include "state.h"
#include "ops.h"

#include <algorithm>
#include <fstream>
#include <iostream>
#include <iterator>
#include <string>

using std::copy;
using std::cout;
using std::ifstream;
using std::istreambuf_iterator;
using std::string;

int main(int argc, char* argv[])
{
    const uint32_t buf_size = 0x60000;
    uint8_t rom_buffer[buf_size];
    if (argc < 2) {
	cout << "Enter ROM filename as an argument.";
	return 0;
    }
    load_rom(argv[1], rom_buffer);
    State state(rom_buffer, buf_size);
}

void load_rom(string filename, uint8_t* rom_buffer)
{
    ifstream rom_file(filename, ifstream::binary);
    copy(istreambuf_iterator<char>(rom_file),
         istreambuf_iterator<char>(),
	 rom_buffer);
}
