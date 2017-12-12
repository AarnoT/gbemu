#include "ops.h"
#include "op_table.h"

#include <cstdint>
#include <iostream>
#include <map>
#include <string>
#include <utility>
#include <vector>

using std::uint8_t;
using std::uint16_t;
using std::uint32_t;
using std::uint64_t;
using std::int8_t;
using std::int16_t;
using std::int32_t;
using std::int64_t;
using std::make_pair;
using std::map;
using std::pair;
using std::string;
using std::vector;

bool check_carry(pair<uint16_t, uint16_t> operands,
		 uint8_t carry_bit, uint8_t flags)
{
    uint16_t num1 = operands.first, num2 = operands.second;
    uint16_t mask = (1 << (carry_bit + 1)) - 1;

    if (flags & FLAG_N != 0) {
        return (int32_t) (num1 & mask) - (int32_t) (num2 & mask) < 0;
    }
    else {
	return (num1 & mask) + (num2 & mask) & (1 << (carry_bit + 1)) != 0;
    }
}

void update_flag(State& state, uint8_t flag_bit, FlagEffect& effect, bool value)
{
    switch (effect) {
    case APPLY:
        state.f |= value ? flag_bit : 0;
	break;
    case SET:
	state.f |= flag_bit;
	break;
    case CLEAR:
	state.f &= ~flag_bit;
	break;
    case IGNORE:
	break;
    }
}

void update_flags(State& state, uint8_t* op_code,
		  pair<uint16_t, uint16_t> operands, bool uint16)
{
    uint16_t num1 = operands.first, num2 = operands.second;

    if (op_code[0] == 0xcb) { /* POP AF */
        state.f = num1 & 0xf0; 
	return;
    }

    Instruction& i = op_code[0] == 0xcb ? ops_cb[op_code[1]] : ops[op_code[0]];
    uint32_t result = i.flags_ZNHC[1] == SET ? num1 - num2 : num1 + num2;
    bool rotate_op = (op_code[0] & 0xe7) == 7 || (op_code[0] == 0xcb && i.flags_ZNHC[3] == APPLY);

    uint8_t half_carry_bit = uint16 ? 11 : 3;
    uint8_t carry_bit = uint16 ? 15 : 7;

    update_flag(state, FLAG_Z, i.flags_ZNHC[0], result == 0);
    update_flag(state, FLAG_N, i.flags_ZNHC[1], false);
    update_flag(state, FLAG_H, i.flags_ZNHC[2], check_carry(operands, half_carry_bit, state.f));
    if (!rotate_op) {
	/* Carry flag is set elsewhere for rotation/shift ops. */
        update_flag(state, FLAG_C, i.flags_ZNHC[3], check_carry(operands, half_carry_bit, state.f));
    }
}

uint16_t uint8_to_uint16(uint8_t high, uint8_t low)
{
    return high << 8 | low;
}

uint16_t read_register_pair(State& state, const string& register_name)
{
    auto& register_pair = state.register_pairs[register_name];
    return uint8_to_uint16(*register_pair.first, *register_pair.second);
}

void write_register_pair(State& state, const string& register_name, uint16_t value)
{
    *state.register_pairs[register_name].first = value >> 8 & 0xff;
    *state.register_pairs[register_name].second = value & 0xff;
}

uint8_t read_8bit_operand(State& state, const string& operand_name, uint8_t* code)
{
    uint8_t value = 0;
    if (state.registers.find(operand_name) != state.registers.end()) {
	value = *state.registers[operand_name];
    } else if (operand_name.compare("(HL)") == 0) {
	uint16_t hl = read_register_pair(state, "HL");
	value = state.read_memory(hl);
    } else if (operand_name.compare("d8") == 0) {
	value = code[1];
    }
    return value;
}

pair<uint16_t, uint16_t> LD(State& state, Instruction& instruction, uint8_t* code)
{
    uint16_t value = 0, num1 = 0, num2 = 0;
    if (state.registers.find(instruction.operand2) != state.registers.end()) {
	value = *state.registers[instruction.operand2];
    } else if (state.register_pairs.find(instruction.operand2) != state.register_pairs.end()) {
	value = read_register_pair(state, instruction.operand2);
    } else if (instruction.operand2.compare("d8") == 0) {
	value = code[1];
    } else if (instruction.operand2.compare("d16") == 0) {
	value = uint8_to_uint16(code[2], code[1]);
    } else if (instruction.operand2.compare("SP") == 0) {
	value = state.sp;
    } else if (instruction.operand2.compare("SP+r8") == 0) {
	value = state.sp + (int8_t) code[1];
	num1 = state.sp;
	num2 = code[1] & 0x7f;
    } else if (instruction.operand2.front() == '(' && instruction.operand2.back() == ')') {
	string location = instruction.operand2.substr(1, instruction.operand2.size() - 2);
	uint16_t addr = 0;
	if (state.register_pairs.find(location) != state.register_pairs.end()) {
	    addr = read_register_pair(state, instruction.operand2);
	} else if (location.compare("HL+") == 0) {
	    addr = read_register_pair(state, instruction.operand2);
	    write_register_pair(state, "HL", read_register_pair(state, "HL") + 1);
	} else if (location.compare("HL-") == 0) {
	    addr = read_register_pair(state, instruction.operand2);
	    write_register_pair(state, "HL", read_register_pair(state, "HL") - 1);
	} else if (location.compare("a16") == 0) {
	    addr = uint8_to_uint16(code[2], code[1]);
	} else if (location.compare("C") == 0) {
	    addr = uint8_to_uint16(0xff, state.c);
	}
	value = state.read_memory(addr);
    }

    if (state.registers.find(instruction.operand1) != state.registers.end()) {
	*state.registers[instruction.operand1] = value;
    } else if (state.register_pairs.find(instruction.operand1) != state.register_pairs.end()) {
	write_register_pair(state, instruction.operand1, value);
    } else if (instruction.operand1.compare("SP") == 0) {
	state.sp = value;
    } else if (instruction.operand1.front() == '(' && instruction.operand1.back() == ')') {
	string location = instruction.operand1.substr(1, instruction.operand1.size() - 2);
	uint16_t addr = 0;
	if (state.register_pairs.find(location) != state.register_pairs.end()) {
	    addr = read_register_pair(state, instruction.operand1);
	} else if (location.compare("HL+") == 0) {
	    addr = read_register_pair(state, instruction.operand1);
	    write_register_pair(state, "HL", read_register_pair(state, "HL") + 1);
	} else if (location.compare("HL-") == 0) {
	    addr = read_register_pair(state, instruction.operand1);
	    write_register_pair(state, "HL", read_register_pair(state, "HL") - 1);
	} else if (location.compare("a16") == 0) {
	    addr = uint8_to_uint16(code[2], code[1]);
	} else if (location.compare("C") == 0) {
	    addr = uint8_to_uint16(0xff, state.c);
	}
	state.write_memory(addr, value);
    }

    return make_pair(num1, num2);
}

pair<uint16_t, uint16_t> LDH(State& state, Instruction& instruction, uint8_t* code)
{
    uint16_t addr = uint8_to_uint16(0xff, code[1]);
    if (instruction.operand1.compare("A") == 0) {
        state.a = state.read_memory(addr);
    } else {
	state.write_memory(addr, state.a);
    }
    return make_pair(0, 0);
}

pair<uint16_t, uint16_t> POP(State& state, Instruction& instruction, uint8_t* code)
{
    uint8_t low = state.read_memory(state.sp++);
    uint8_t high = state.read_memory(state.sp++);
    uint16_t value = uint8_to_uint16(high, low);
    write_register_pair(state, instruction.operand1, value);
    return make_pair(0, 0);
}

pair<uint16_t, uint16_t> PUSH(State& state, Instruction& instruction, uint8_t* code)
{
    uint16_t value = read_register_pair(state, instruction.operand1);
    state.write_memory(state.sp--, value >> 8 & 0xff);
    state.write_memory(state.sp--, value & 0xff);
    return make_pair(0, 0);
}

pair<uint16_t, uint16_t> INC(State& state, Instruction& instruction, uint8_t* code)
{
    uint16_t num1 = 0, num2 = 1;
    if (state.registers.find(instruction.operand1) != state.registers.end()) {
	num1 = *state.registers[instruction.operand1];
	(*state.registers[instruction.operand1])++;
    } else if (state.register_pairs.find(instruction.operand1) != state.register_pairs.end()) {
	uint16_t value = read_register_pair(state, instruction.operand1);
	write_register_pair(state, instruction.operand1, value + 1);
    } else if (instruction.operand1.compare("SP") == 0) {
	state.sp++;
    } else if (instruction.operand1.compare("(HL)") == 0) {
	uint16_t hl = read_register_pair(state, "HL");
	uint16_t value = state.read_memory(hl);
	state.write_memory(hl, value + 1);
	num1 = value;
    }
    return make_pair(num1, num2);
}

pair<uint16_t, uint16_t> DEC(State& state, Instruction& instruction, uint8_t* code)
{
    uint16_t num1 = 0, num2 = 1;
    if (state.registers.find(instruction.operand1) != state.registers.end()) {
	num1 = *state.registers[instruction.operand1];
	(*state.registers[instruction.operand1])--;
    } else if (state.register_pairs.find(instruction.operand1) != state.register_pairs.end()) {
	uint16_t value = read_register_pair(state, instruction.operand1);
	write_register_pair(state, instruction.operand1, value - 1);
    } else if (instruction.operand1.compare("SP") == 0) {
	state.sp--;
    } else if (instruction.operand1.compare("(HL)") == 0) {
	uint16_t hl = read_register_pair(state, "HL");
	uint16_t value = state.read_memory(hl);
	state.write_memory(hl, value - 1);
	num1 = value;
    }
    return make_pair(num1, num2);
}

pair<uint16_t, uint16_t> DAA(State& state, Instruction& instruction, uint8_t* code)
{
    uint16_t num1 = state.a, num2 = 0;
    if (state.f & FLAG_C || ((state.f & FLAG_N) == 0 && state.a > 0x99)) {
	num2 += 0x60;
    }
    if (state.f & FLAG_H || ((state.f & FLAG_N) == 0 && (state.a & 0xf) > 0x9)) {
	num2 += 6;
    }
    state.a += state.f & FLAG_N ? -num2 : num2;
    return make_pair(num1, num2);
}

pair<uint16_t, uint16_t> CPL(State& state, Instruction& instruction, uint8_t* code)
{
    state.a = ~state.a;
    return make_pair(state.a, 0);
}

pair<uint16_t, uint16_t> ADD(State& state, Instruction& instruction, uint8_t* code)
{
    uint16_t num1 = 0, num2 = 0;
    if (state.register_pairs.find(instruction.operand2) != state.register_pairs.end()) {
	num2 = read_register_pair(state, instruction.operand2);
    } else if (instruction.operand2.compare("SP") == 0) {
        num2 = state.sp;
    } else {
	num2 = read_8bit_operand(state, instruction.operand2, code);
    }

    if (instruction.operand1.compare("HL") == 0) {
	num1 = read_register_pair(state, "HL");
	write_register_pair(state, "HL", num1 + num2);
    } else {
	num1 = state.a;
	state.a += num2;
    }

    return make_pair(num1, num2);
}

pair<uint16_t, uint16_t> ADC(State& state, Instruction& instruction, uint8_t* code)
{
    uint16_t num1 = state.a;
    uint16_t num2 = read_8bit_operand(state, instruction.operand2, code);
    num2 += state.f & FLAG_C ? 1 : 0;
    state.a += num2;
    return make_pair(num1, num2);
}

pair<uint16_t, uint16_t> SUB(State& state, Instruction& instruction, uint8_t* code)
{
    uint16_t num1 = state.a;
    uint16_t num2 = read_8bit_operand(state, instruction.operand1, code);
    state.a -= num2;
    return make_pair(num1, num2);
}

pair<uint16_t, uint16_t> SBC(State& state, Instruction& instruction, uint8_t* code)
{
    uint16_t num1 = state.a;
    uint16_t num2 = read_8bit_operand(state, instruction.operand2, code);
    num2 += state.f & FLAG_C ? 1 : 0;
    state.a -= num2;
    return make_pair(num1, num2);
}

pair<uint16_t, uint16_t> AND(State& state, Instruction& instruction, uint8_t* code)
{
    uint16_t num1 = state.a;
    uint16_t num2 = read_8bit_operand(state, instruction.operand1, code);
    state.a &= num2;
    return make_pair(state.a, 0);
}

pair<uint16_t, uint16_t> XOR(State& state, Instruction& instruction, uint8_t* code)
{
    uint16_t num1 = state.a;
    uint16_t num2 = read_8bit_operand(state, instruction.operand1, code);
    state.a ^= num2;
    return make_pair(state.a, 0);
}

pair<uint16_t, uint16_t> OR(State& state, Instruction& instruction, uint8_t* code)
{
    uint16_t num1 = state.a;
    uint16_t num2 = read_8bit_operand(state, instruction.operand1, code);
    state.a |= num2;
    return make_pair(state.a, 0);
}

pair<uint16_t, uint16_t> CP(State& state, Instruction& instruction, uint8_t* code)
{
    uint16_t num1 = state.a;
    uint16_t num2 = read_8bit_operand(state, instruction.operand1, code);
    return make_pair(num1, num2);
}
