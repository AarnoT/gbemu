#include "debug.h"
#include "instruction.h"
#include "state.h"

#include <algorithm>
#include <cstdint>
#include <deque>
#include <iostream>
#include <vector>

using std::cout;
using std::deque;
using std::find;
using std::hex;
using std::uint8_t;
using std::uint16_t;
using std::vector;
 
deque<uint16_t> recent_jumps;

void add_jump(std::uint16_t addr)
{
    auto jump = find(recent_jumps.begin(), recent_jumps.end(), addr);
    if (jump != recent_jumps.end()) {
        recent_jumps.erase(jump);
    }

    recent_jumps.push_front(addr);
    if (recent_jumps.size() > 10) {
        recent_jumps.pop_back();
    }
}

void print_debug_info(State& state, Instruction& instruction, vector<uint8_t> op_code)
{
    cout << "Op code: ";
    for (uint8_t b : op_code) {
        cout << hex << (uint16_t) b << " ";
    }
    cout << "Mnemonic: " << instruction.name;
    if (instruction.operand_count >= 1) {cout << " " << instruction.operand1;}
    if (instruction.operand_count >= 2) {cout << " " << instruction.operand2;}
    cout << "\n";
    cout << "PC: " << hex << state.pc << " SP: " << state.sp << "\n";
    cout << "BC: " << hex << ((state.b << 8) | state.c) << " ";
    cout << "DE: " << hex << ((state.d << 8) | state.e) << " ";
    cout << "HL: " << hex << ((state.h << 8) | state.l) << "\n";
    cout << "A: " << hex << (uint16_t) state.a << " ";
    cout << "Flags: " << ((state.f & 0x80) != 0) << ((state.f & 0x40) != 0)
	              << ((state.f & 0x20) != 0) << ((state.f & 0x10) != 0) <<  "\n";
    cout << hex << "LCDC: " << (uint16_t) state.read_memory(0xff40) << " ";
    cout << hex << "STAT: " << (uint16_t) state.read_memory(0xff41) << " ";
    cout << hex << "LY: " << (uint16_t) state.read_memory(0xff44) << "\n\n";
}
