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

uint16_t read_operand(State& state, const string& operand_name, uint8_t* op_code)
{
    uint16_t value = 0;
    if (state.registers.find(operand_name) != state.registers.end()) {
	value = *state.registers[operand_name];
    } else if (state.register_pairs.find(operand_name) != state.register_pairs.end()) {
	value = read_register_pair(state, operand_name);
    } else if (operand_name == "d8") {
	value = op_code[1];
    } else if (operand_name == "d16") {
	value = uint8_to_uint16(op_code[2], op_code[1]);
    } else if (operand_name == "a16") {
	value = uint8_to_uint16(op_code[2], op_code[1]);
    } else if (operand_name == "r8") {
	/* Needs to be converted to signed when used */
	value = op_code[1];
    } else if (operand_name == "SP") {
	value = state.sp;
    } else if (operand_name == "SP+r8") {
	value = state.sp + (int8_t) op_code[1];
    } else if (operand_name.length() == 3 && operand_name[2] == 'H') {
	sscanf(operand_name.substr(0, 2).c_str(), "%x", value);
    } else if (operand_name.front() == '(' && operand_name.back() == ')') {
	string location = operand_name.substr(1, operand_name.length() - 2);
	uint16_t addr = 0;
	if (state.register_pairs.find(location) != state.register_pairs.end()) {
	    addr = read_register_pair(state, operand_name);
	} else if (location == "HL+") {
	    addr = read_register_pair(state, operand_name);
	    write_register_pair(state, "HL", read_register_pair(state, "HL") + 1);
	} else if (location == "HL-") {
	    addr = read_register_pair(state, operand_name);
	    write_register_pair(state, "HL", read_register_pair(state, "HL") - 1);
	} else if (location == "a8") {
	    addr = uint8_to_uint16(0xff, op_code[1]);
	} else if (location == "a16") {
	    addr = uint8_to_uint16(op_code[2], op_code[1]);
	} else if (location == "C") {
	    addr = uint8_to_uint16(0xff, state.c);
	}
	value = state.read_memory(addr);
    }
    return value;
}

void write_operand(State& state, const string& operand_name, uint8_t* op_code, uint16_t value)
{
    if (state.registers.find(operand_name) != state.registers.end()) {
	*state.registers[operand_name] = value;
    } else if (state.register_pairs.find(operand_name) != state.register_pairs.end()) {
	write_register_pair(state, operand_name, value);
    } else if (operand_name == "SP") {
	state.sp = value;
    } else if (operand_name.front() == '(' && operand_name.back() == ')') {
	string location = operand_name.substr(1, operand_name.size() - 2);
	uint16_t addr = 0;
	if (state.register_pairs.find(location) != state.register_pairs.end()) {
	    addr = read_register_pair(state, operand_name);
	} else if (location == "HL+") {
	    addr = read_register_pair(state, operand_name);
	    write_register_pair(state, "HL", read_register_pair(state, "HL") + 1);
	} else if (location == "HL-") {
	    addr = read_register_pair(state, operand_name);
	    write_register_pair(state, "HL", read_register_pair(state, "HL") - 1);
	} else if (location == "a8") {
	    addr = uint8_to_uint16(0xff, op_code[1]);
	} else if (location == "a16") {
	    addr = uint8_to_uint16(op_code[2], op_code[1]);
	} else if (location == "C") {
	    addr = uint8_to_uint16(0xff, state.c);
	}
	state.write_memory(addr, value);
    }
}

bool check_condition(State& state, string& condition_code)
{
	bool condition = false;
	switch (condition_code.back()) {
	case 'Z': condition = state.f & FLAG_Z != 0; break;
	case 'C': condition = state.f & FLAG_C != 0; break;
	}
	if (condition_code.front() == 'N') {condition = !condition;}
	return condition;
}

uint16_t pop_from_stack(State& state)
{
    uint8_t low = state.read_memory(state.sp++);
    uint8_t high = state.read_memory(state.sp++);
    return uint8_to_uint16(high, low);
}

void push_onto_stack(State& state, uint16_t value)
{
    state.write_memory(state.sp--, value >> 8 & 0xff);
    state.write_memory(state.sp--, value & 0xff);
}

pair<uint16_t, uint16_t> LD(State& state, Instruction& instruction, uint8_t* op_code)
{
    uint16_t num1 = 0, num2 = 0;
    uint16_t value = read_operand(state, instruction.operand2, op_code);
    write_operand(state, instruction.operand1, op_code, value);

    if (instruction.operand2.compare("SP+r8") == 0) {
	num1 = state.sp;
	num2 = op_code[1] & 0x7f;
    }

    return make_pair(num1, num2);
}

pair<uint16_t, uint16_t> POP(State& state, Instruction& instruction, uint8_t* op_code)
{
    uint16_t value = pop_from_stack(state);
    write_register_pair(state, instruction.operand1, value);
    return make_pair(0, 0);
}

pair<uint16_t, uint16_t> PUSH(State& state, Instruction& instruction, uint8_t* op_code)
{
    uint16_t value = read_register_pair(state, instruction.operand1);
    push_onto_stack(state, value);
    return make_pair(0, 0);
}

pair<uint16_t, uint16_t> INC(State& state, Instruction& instruction, uint8_t* op_code)
{
    uint16_t num1 = 0, num2 = 1;
    num1 = read_operand(state, instruction.operand1, op_code);
    write_operand(state, instruction.operand1, op_code, num1 + 1);

    return make_pair(num1, num2);
}

pair<uint16_t, uint16_t> DEC(State& state, Instruction& instruction, uint8_t* op_code)
{
    uint16_t num1 = 0, num2 = 1;
    num1 = read_operand(state, instruction.operand1, op_code);
    write_operand(state, instruction.operand1, op_code, num1 - 1);

    return make_pair(num1, num2);
}

pair<uint16_t, uint16_t> DAA(State& state, Instruction& instruction, uint8_t* op_code)
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

pair<uint16_t, uint16_t> CPL(State& state, Instruction& instruction, uint8_t* op_code)
{
    state.a = ~state.a;
    return make_pair(state.a, 0);
}

pair<uint16_t, uint16_t> ADD(State& state, Instruction& instruction, uint8_t* op_code)
{
    uint16_t value = 0, num1 = 0, num2 = 0;
    num1 = read_operand(state, instruction.operand1, op_code);

    if (instruction.operand2 == "r8") {
	value = num1 + (int8_t) num2;
	num2 &= 0x7f;
    } else {
	value = num1 + num2;
    }

    write_operand(state, instruction.operand1, op_code, value);

    return make_pair(num1, num2);
}

pair<uint16_t, uint16_t> ADC(State& state, Instruction& instruction, uint8_t* op_code)
{
    uint16_t num1 = state.a;
    uint16_t num2 = read_operand(state, instruction.operand2, op_code);
    num2 += state.f & FLAG_C ? 1 : 0;
    state.a += num2;
    return make_pair(num1, num2);
}

pair<uint16_t, uint16_t> SUB(State& state, Instruction& instruction, uint8_t* op_code)
{
    uint16_t num1 = state.a;
    uint16_t num2 = read_operand(state, instruction.operand1, op_code);
    state.a -= num2;
    return make_pair(num1, num2);
}

pair<uint16_t, uint16_t> SBC(State& state, Instruction& instruction, uint8_t* op_code)
{
    uint16_t num1 = state.a;
    uint16_t num2 = read_operand(state, instruction.operand2, op_code);
    num2 += state.f & FLAG_C ? 1 : 0;
    state.a -= num2;
    return make_pair(num1, num2);
}

pair<uint16_t, uint16_t> AND(State& state, Instruction& instruction, uint8_t* op_code)
{
    uint16_t num1 = state.a;
    uint16_t num2 = read_operand(state, instruction.operand1, op_code);
    state.a &= num2;
    return make_pair(state.a, 0);
}

pair<uint16_t, uint16_t> XOR(State& state, Instruction& instruction, uint8_t* op_code)
{
    uint16_t num1 = state.a;
    uint16_t num2 = read_operand(state, instruction.operand1, op_code);
    state.a ^= num2;
    return make_pair(state.a, 0);
}

pair<uint16_t, uint16_t> OR(State& state, Instruction& instruction, uint8_t* op_code)
{
    uint16_t num1 = state.a;
    uint16_t num2 = read_operand(state, instruction.operand1, op_code);
    state.a |= num2;
    return make_pair(state.a, 0);
}

pair<uint16_t, uint16_t> CP(State& state, Instruction& instruction, uint8_t* op_code)
{
    uint16_t num1 = state.a;
    uint16_t num2 = read_operand(state, instruction.operand1, op_code);
    return make_pair(num1, num2);
}

pair<uint16_t, uint16_t> JR(State& state, Instruction& instruction, uint8_t* op_code)
{
    bool jump = instruction.operand_count == 0;
    jump = jump || check_condition(state, instruction.operand1);

    if (jump) {
        state.pc += (int8_t) op_code[1];
    }
}

pair<uint16_t, uint16_t> JP(State& state, Instruction& instruction, uint8_t* op_code)
{
    bool jump = instruction.operand_count == 1;
    jump = jump || check_condition(state, instruction.operand1);

    if (jump) {
        state.pc = uint8_to_uint16(op_code[2], op_code[1]);
    }
}

pair<uint16_t, uint16_t> RET(State& state, Instruction& instruction, uint8_t* op_code)
{
    bool jump = instruction.operand_count == 0;
    jump = jump || check_condition(state, instruction.operand1);

    if (jump) {
        state.pc = pop_from_stack(state);
    }
}

pair<uint16_t, uint16_t> CALL(State& state, Instruction& instruction, uint8_t* op_code)
{
    bool jump = instruction.operand_count == 1;
    jump = jump || check_condition(state, instruction.operand1);

    if (jump) {
        push_onto_stack(state, state.pc);
        state.pc = uint8_to_uint16(op_code[2], op_code[1]);
    }
}
