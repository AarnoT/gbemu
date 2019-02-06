#include "ops.h"
#include "instruction.h"
#include "op_table.h"
#include "state.h"

#include <cinttypes>
#include <cstdint>
#include <iostream>
#include <map>
#include <unordered_set>
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
using std::cout;
using std::hex;
using std::make_pair;
using std::pair;
using std::unordered_map;
using std::unordered_set;
using std::string;
using std::vector;

unordered_map<string, OpFunction> op_functions {
    {"NOP", &NOP}, {"STOP", &STOP}, {"JR", &JR}, {"LD", &LD}, {"INC", &INC}, {"DEC", &DEC},
    {"RLCA", &RLCA}, {"RLA", &RLA}, {"DAA", &DAA}, {"SCF", &NOP}, {"ADD", &ADD}, 
    {"RRCA", &RRCA}, {"RRA", &RRA}, {"CPL", &CPL}, {"CCF", &CCF}, {"ADC", &ADC},
    {"HALT", &HALT}, {"SUB", &SUB}, {"SBC", &SBC}, {"AND", &AND}, {"XOR", &XOR}, {"OR", OR},
    {"CP", &CP}, {"RET", &RET}, {"LDH", &LD}, {"POP", &POP}, {"JP", &JP}, {"DI", &DI},
    {"CALL", &CALL}, {"PUSH", &PUSH}, {"RST", &RST}, {"RETI", &RETI}, {"EI", &EI},
    {"RLC", &RLC}, {"RRC", &RRC}, {"RL", &RL}, {"RR", &RR}, {"SLA", &SLA}, {"SRA", &SRA},
    {"SWAP", &SWAP}, {"SRL", &SRL}, {"BIT", &BIT}, {"RES", &RES}, {"SET", &SET} 
};

unordered_set<uint8_t> invalid_ops = {0xd3, 0xdd, 0xdb, 0xe3, 0xe4, 0xeb,
	                              0xec, 0xed, 0xf4, 0xfc, 0xfd};
unordered_set<uint8_t> ops_16b {0x09, 0x19, 0x29, 0x39};

uint32_t execute_op(State& state)
{
    uint8_t op = state.read_memory(state.pc);
    vector<uint8_t> op_code(0);
    Instruction instruction = ops[0];

    if (op == 0xcb) {
	op_code.resize(2);
	op_code[0] = op;
	op_code[1] = state.read_memory(state.pc + 1);
        instruction = ops_cb[op_code[1]];
    } else {
        instruction = ops[op];
	op_code.resize(instruction.bytes);
	op_code[0] = op;
	for (uint8_t i = 1; i < instruction.bytes; i++) {
	    op_code[i] = state.read_memory(state.pc + i);
	}
    }

    if (invalid_ops.find(op_code[0]) != invalid_ops.end()) {
	cout << "[WARNING]: Invalid instruction encountered at " << hex << state.pc << ".\n";
    }

    if (!address_executable(state.pc)) {
	cout << "[WARNING]: PC at unexecutable address: " << hex << state.pc << ".\n";
    }

    state.pc += instruction.bytes;
    uint16_t prev_pc = state.pc;

    bool is_16_bit = ops_16b.find(op_code[0]) != ops_16b.end();

    auto operands = op_functions[instruction.name](state, instruction, op_code);
    update_flags(state, op_code, operands, is_16_bit); 
    state.instructions_executed++;

    if (state.pc != prev_pc && instruction.branch_cycles != 0) {
        return instruction.branch_cycles;
    } else {
        uint32_t gdma_cycles = 8 * 4 * state.prev_gdma_len;
	if (state.double_speed) {gdma_cycles *= 2;}
	state.prev_gdma_len = 0;
	return instruction.cycles + gdma_cycles;
    }
}

bool address_executable(uint16_t addr)
{
    if (addr >= 0x104 && addr <= 0x14f) {
        return false;
    } else if (addr >= 0xe000 && addr <= 0xfdff) {
        return false;
    } else if (addr >= 0xfe00 && addr <= 0xfe9f) {
	return false;
    } else if (addr >= 0xfea0 && addr <= 0xfeff) {
        return false;
    } else if (addr >= 0xff00 && addr <= 0xff7f) {
        return false;
    } else if (addr == 0xffff) {
        return false;
    } else {
        return true;
    }
}

bool check_carry(pair<uint16_t, uint16_t> operands,
		 uint8_t carry_bit, uint8_t flags)
{
    uint16_t num1 = operands.first, num2 = operands.second;
    uint16_t mask = (1 << (carry_bit + 1)) - 1;

    if ((flags & FLAG_N) != 0) {
        return (int32_t) (num1 & mask) - (int32_t) (num2 & mask) < 0;
    }
    else {
	return (((num1 & mask) + (num2 & mask)) & (1 << (carry_bit + 1))) != 0;
    }
}

void update_flag(State& state, uint8_t flag_bit, FlagEffect& effect, bool value)
{
    switch (effect) {
    case FlagEffect::APPLY:
	if (value) {
	    state.f |= flag_bit;
	} else {
            state.f &= ~flag_bit;
	}
	break;
    case FlagEffect::SET:
	state.f |= flag_bit;
	break;
    case FlagEffect::CLEAR:
	state.f &= ~flag_bit;
	break;
    case FlagEffect::IGNORE:
	break;
    }
}

void update_flags(State& state, vector<uint8_t>& op_code,
		  pair<uint16_t, uint16_t> operands, bool is_16_bit)
{
    uint16_t num1 = operands.first, num2 = operands.second;

    if (op_code[0] == 0xf1) { /* POP AF */
	return;
    }

    Instruction& i = op_code[0] == 0xcb ? ops_cb[op_code[1]] : ops[op_code[0]];
    uint32_t result = i.flags_ZNHC[1] == FlagEffect::SET ? num1 - num2 : num1 + num2;
    if (i.name == "DAA") {
	result = (state.f & FLAG_N) ? num1 - num2 : num1 + num2;
    }

    bool rotate_op = (op_code[0] & 0xe7) == 7 || (op_code[0] == 0xcb && i.flags_ZNHC[3] == FlagEffect::APPLY);

    uint8_t half_carry_bit = is_16_bit ? 11 : 3;
    uint8_t carry_bit = is_16_bit ? 15 : 7;

    update_flag(state, FLAG_Z, i.flags_ZNHC[0], (result & (is_16_bit ? 0xffff : 0xff)) == 0);
    update_flag(state, FLAG_N, i.flags_ZNHC[1], false);

    if (i.name == "ADC" || i.name == "SBC") {
        if ((num2 & 0xff) == 0 && state.f & FLAG_C) {
	    state.f |= FLAG_H | FLAG_C;
	    return;
	} else if ((num2 & 0xf) == 0 && state.f & FLAG_C) {
            update_flag(state, FLAG_C, i.flags_ZNHC[3], check_carry(operands, carry_bit, state.f));
	    state.f |= FLAG_H;
	    return;
	}
    }

    update_flag(state, FLAG_H, i.flags_ZNHC[2], check_carry(operands, half_carry_bit, state.f));
    if (!rotate_op && i.name != "CCF" && i.name != "DAA" && i.name != "SRA") {
	/* Carry flag is set elsewhere for rotation/shift ops. */
        update_flag(state, FLAG_C, i.flags_ZNHC[3], check_carry(operands, carry_bit, state.f));
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

uint16_t read_operand(State& state, const string& operand_name, vector<uint8_t>& op_code)
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
	sscanf(operand_name.substr(0, 2).c_str(), "%" SCNx16, &value);
    } else if (operand_name.front() == '(' && operand_name.back() == ')') {
	string location = operand_name.substr(1, operand_name.length() - 2);
	uint16_t addr = 0;
	if (state.register_pairs.find(location) != state.register_pairs.end()) {
	    addr = read_register_pair(state, location.c_str());
	} else if (location == "HL+") {
	    addr = read_register_pair(state, "HL");
	    write_register_pair(state, "HL", read_register_pair(state, "HL") + 1);
	} else if (location == "HL-") {
	    addr = read_register_pair(state, "HL");
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

void write_operand(State& state, const string& operand_name, vector<uint8_t>& op_code, uint16_t value)
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
	    addr = read_register_pair(state, location.c_str());
	} else if (location == "HL+") {
	    addr = read_register_pair(state, "HL");
	    write_register_pair(state, "HL", read_register_pair(state, "HL") + 1);
	} else if (location == "HL-") {
	    addr = read_register_pair(state, "HL");
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
	case 'Z': condition = (state.f & FLAG_Z) != 0; break;
	case 'C': condition = (state.f & FLAG_C) != 0; break;
	}
	if (condition_code.front() == 'N') {condition = !condition;}
	return condition;
}

uint16_t pop_from_stack(State& state)
{
    uint8_t low = state.read_memory(state.sp++);
    uint8_t high = state.read_memory(state.sp++);
    return uint8_to_uint16(high, low);

    if (state.stack_depth == 0) {
	cout << "[WARNING]: Stack depth too low at " << hex << state.pc << ".\n";
    } else {
        state.stack_depth--;
    }
}

void push_onto_stack(State& state, uint16_t value)
{
    state.write_memory(--state.sp, value >> 8 & 0xff);
    state.write_memory(--state.sp, value & 0xff);

    state.stack_depth++;
}

pair<uint16_t, uint16_t> LD(State& state, Instruction& instruction, vector<uint8_t>& op_code)
{
    uint16_t num1 = 0, num2 = 0;
    uint16_t value = read_operand(state, instruction.operand2, op_code);

    if (op_code[0] == 0x8) {
        uint16_t addr = uint8_to_uint16(op_code[2], op_code[1]);
	state.write_memory(addr, state.sp & 0xff);
	state.write_memory(addr + 1, (state.sp >> 8) & 0xff);
    } else {
        write_operand(state, instruction.operand1, op_code, value);
    }

    if (op_code[0] == 0xf8) {
        num1 = state.sp;
	num2 = op_code[1];
    }
    return make_pair(num1, num2);
}

pair<uint16_t, uint16_t> POP(State& state, Instruction& instruction, vector<uint8_t>& op_code)
{
    uint16_t value = pop_from_stack(state);
    write_register_pair(state, (const string) instruction.operand1, value);
    if (op_code[0] == 0xf1) {state.f &= 0xf0;}
    return make_pair(0, 0);
}

pair<uint16_t, uint16_t> PUSH(State& state, Instruction& instruction, vector<uint8_t>& op_code)
{
    uint16_t value = read_register_pair(state, (const string) instruction.operand1);
    push_onto_stack(state, value);
    return make_pair(0, 0);
}

pair<uint16_t, uint16_t> INC(State& state, Instruction& instruction, vector<uint8_t>& op_code)
{
    uint16_t num1 = 0, num2 = 1;
    num1 = read_operand(state, instruction.operand1, op_code);
    write_operand(state, instruction.operand1, op_code, num1 + 1);

    return make_pair(num1, num2);
}

pair<uint16_t, uint16_t> DEC(State& state, Instruction& instruction, vector<uint8_t>& op_code)
{
    uint16_t num1 = 0, num2 = 1;
    num1 = read_operand(state, instruction.operand1, op_code);
    write_operand(state, instruction.operand1, op_code, num1 - 1);

    return make_pair(num1, num2);
}

pair<uint16_t, uint16_t> DAA(State& state, Instruction& instruction, vector<uint8_t>& op_code)
{
    uint16_t num1 = state.a, num2 = 0;
    if (state.f & FLAG_C || ((state.f & FLAG_N) == 0 && state.a > 0x99)) {
	num2 += 0x60;
	state.f |= FLAG_C;
    }
    if (state.f & FLAG_H || ((state.f & FLAG_N) == 0 && (state.a & 0xf) > 0x9)) {
	num2 += 6;
    }
    state.a += state.f & FLAG_N ? -num2 : num2;
    return make_pair(num1, num2);
}

pair<uint16_t, uint16_t> CPL(State& state, Instruction& instruction, vector<uint8_t>& op_code)
{
    state.a = ~state.a;
    return make_pair(state.a, 0);
}

pair<uint16_t, uint16_t> ADD(State& state, Instruction& instruction, vector<uint8_t>& op_code)
{
    uint16_t value = 0;
    uint16_t num1 = read_operand(state, instruction.operand1, op_code);
    uint16_t num2 = read_operand(state, instruction.operand2, op_code);

    if (instruction.operand2 == "r8") {
	value = num1 + (int8_t) num2;
    } else {
	value = num1 + num2;
    }

    write_operand(state, instruction.operand1, op_code, value);

    return make_pair(num1, num2);
}

pair<uint16_t, uint16_t> ADC(State& state, Instruction& instruction, vector<uint8_t>& op_code)
{
    uint16_t num1 = state.a;
    uint16_t num2 = read_operand(state, instruction.operand2, op_code);
    num2 += (state.f & FLAG_C) ? 1 : 0;
    state.a += num2;
    return make_pair(num1, num2);
}

pair<uint16_t, uint16_t> SUB(State& state, Instruction& instruction, vector<uint8_t>& op_code)
{
    uint16_t num1 = state.a;
    uint16_t num2 = read_operand(state, instruction.operand1, op_code);
    state.a -= num2;
    return make_pair(num1, num2);
}

pair<uint16_t, uint16_t> SBC(State& state, Instruction& instruction, vector<uint8_t>& op_code)
{
    uint16_t num1 = state.a;
    uint16_t num2 = read_operand(state, instruction.operand2, op_code);
    num2 += (state.f & FLAG_C) ? 1 : 0;
    state.a -= num2;
    return make_pair(num1, num2);
}

pair<uint16_t, uint16_t> AND(State& state, Instruction& instruction, vector<uint8_t>& op_code)
{
    uint16_t num2 = read_operand(state, instruction.operand1, op_code);
    state.a &= num2;
    return make_pair(state.a, 0);
}

pair<uint16_t, uint16_t> XOR(State& state, Instruction& instruction, vector<uint8_t>& op_code)
{
    uint16_t num2 = read_operand(state, instruction.operand1, op_code);
    state.a ^= num2;
    return make_pair(state.a, 0);
}

pair<uint16_t, uint16_t> OR(State& state, Instruction& instruction, vector<uint8_t>& op_code)
{
    uint16_t num2 = read_operand(state, instruction.operand1, op_code);
    state.a |= num2;
    return make_pair(state.a, 0);
}

pair<uint16_t, uint16_t> CP(State& state, Instruction& instruction, vector<uint8_t>& op_code)
{
    uint16_t num1 = state.a;
    uint16_t num2 = read_operand(state, instruction.operand1, op_code);
    return make_pair(num1, num2);
}

pair<uint16_t, uint16_t> JR(State& state, Instruction& instruction, vector<uint8_t>& op_code)
{
    bool jump = instruction.operand_count == 1;
    jump = jump || check_condition(state, instruction.operand1);

    if (jump) {
        state.pc += (int8_t) op_code[1];
    }
    return make_pair(0, 0);
}

pair<uint16_t, uint16_t> JP(State& state, Instruction& instruction, vector<uint8_t>& op_code)
{
    bool jump = instruction.operand_count == 1;
    jump = jump || check_condition(state, instruction.operand1);

    if (jump) {
	if (instruction.operand1 == "(HL)") {
	    state.pc = read_register_pair(state, "HL");
	} else {
            state.pc = uint8_to_uint16(op_code[2], op_code[1]);
	}
    }
    return make_pair(0, 0);
}

pair<uint16_t, uint16_t> RET(State& state, Instruction& instruction, vector<uint8_t>& op_code)
{
    bool jump = instruction.operand_count == 0;
    jump = jump || check_condition(state, instruction.operand1);

    if (jump) {
        state.pc = pop_from_stack(state);
    }
    return make_pair(0, 0);
}

pair<uint16_t, uint16_t> RETI(State& state, Instruction& instruction, vector<uint8_t>& op_code)
{
    bool jump = instruction.operand_count == 0;
    jump = jump || check_condition(state, instruction.operand1);

    if (jump) {
        state.pc = pop_from_stack(state);
    }
    state.interrupts_enabled = true;
    return make_pair(0, 0);
}

pair<uint16_t, uint16_t> CALL(State& state, Instruction& instruction, vector<uint8_t>& op_code)
{
    bool jump = instruction.operand_count == 1;
    jump = jump || check_condition(state, instruction.operand1);

    if (jump) {
        push_onto_stack(state, state.pc);
        state.pc = uint8_to_uint16(op_code[2], op_code[1]);
    }
    return make_pair(0, 0);
}

pair<uint16_t, uint16_t> RST(State& state, Instruction& instruction, vector<uint8_t>& op_code)
{
    push_onto_stack(state, state.pc);
    state.pc = read_operand(state, instruction.operand1, op_code);
    return make_pair(0, 0);
}

pair<uint16_t, uint16_t> RLCA(State& state, Instruction& instruction, vector<uint8_t>& op_code)
{
    uint8_t bit7 = (state.a & 0x80) >> 7;
    if (bit7 != 0) {
        state.f |= FLAG_C;
    } else {
        state.f &= ~FLAG_C;
    }
    state.a = (state.a << 1) | bit7;
    return make_pair(0, 0);
}

pair<uint16_t, uint16_t> RLA(State& state, Instruction& instruction, vector<uint8_t>& op_code)
{
    uint8_t old_a = state.a;
    state.a = (state.a << 1) | ((state.f & FLAG_C) ? 1 : 0);
    if (old_a & 0x80) {
        state.f |= FLAG_C;
    } else {
        state.f &= ~FLAG_C;
    }
    return make_pair(0, 0);
}

pair<uint16_t, uint16_t> RRCA(State& state, Instruction& instruction, vector<uint8_t>& op_code)
{
    uint8_t bit0 = (state.a & 1) << 7;
    if (bit0 != 0) {
        state.f |= FLAG_C;
    } else {
        state.f &= ~FLAG_C;
    }
    state.a = (state.a >> 1) | bit0;
    return make_pair(0, 0);
}

pair<uint16_t, uint16_t> RRA(State& state, Instruction& instruction, vector<uint8_t>& op_code)
{
    uint8_t old_a = state.a;
    state.a = (state.a >> 1) | ((state.f & FLAG_C) ? 0x80 : 0);
    if (old_a & 1) {
        state.f |= FLAG_C;
    } else {
        state.f &= ~FLAG_C;
    }
    return make_pair(0, 0);
}

pair<uint16_t, uint16_t> RLC(State& state, Instruction& instruction, vector<uint8_t>& op_code)
{
    uint8_t value = read_operand(state, instruction.operand1, op_code);

    uint8_t bit7 = (value & 0x80) >> 7;
    if (bit7 != 0) {
        state.f |= FLAG_C;
    } else {
        state.f &= ~FLAG_C;
    }
    value = (value << 1) | bit7;

    write_operand(state, instruction.operand1, op_code, value);
    return make_pair(value, 0);
}

pair<uint16_t, uint16_t> RRC(State& state, Instruction& instruction, vector<uint8_t>& op_code)
{
    uint8_t value = read_operand(state, instruction.operand1, op_code);

    uint8_t bit0 = (value & 1) << 7;
    if (bit0 != 0) {
        state.f |= FLAG_C;
    } else {
        state.f &= ~FLAG_C;
    }
    value = (value >> 1) | bit0;

    write_operand(state, instruction.operand1, op_code, value);
    return make_pair(value, 0);
}

pair<uint16_t, uint16_t> RL(State& state, Instruction& instruction, vector<uint8_t>& op_code)
{
    uint8_t value = read_operand(state, instruction.operand1, op_code);
    uint8_t old_value = value;
 
    value = (value << 1) | ((state.f & FLAG_C) ? 1 : 0);
    if (old_value & 0x80) {
        state.f |= FLAG_C;
    } else {
        state.f &= ~FLAG_C;
    }

    write_operand(state, instruction.operand1, op_code, value);
    return make_pair(value, 0);
}

pair<uint16_t, uint16_t> RR(State& state, Instruction& instruction, vector<uint8_t>& op_code)
{
    uint8_t value = read_operand(state, instruction.operand1, op_code);
    uint8_t old_value = value;
 
    value = (value >> 1) | ((state.f & FLAG_C) ? 0x80 : 0);
    if (old_value & 1) {
        state.f |= FLAG_C;
    } else {
        state.f &= ~FLAG_C;
    }

    write_operand(state, instruction.operand1, op_code, value);
    return make_pair(value, 0);
}

pair<uint16_t, uint16_t> SLA(State& state, Instruction& instruction, vector<uint8_t>& op_code)
{
    uint8_t value = read_operand(state, instruction.operand1, op_code);

    if (value & 0x80) {
        state.f |= FLAG_C;
    } else {
        state.f &= ~FLAG_C;
    }
    value = value << 1;

    write_operand(state, instruction.operand1, op_code, value);
    return make_pair(value, 0);
}

pair<uint16_t, uint16_t> SRA(State& state, Instruction& instruction, vector<uint8_t>& op_code)
{
    uint8_t value = read_operand(state, instruction.operand1, op_code);

    if (value & 1) {
        state.f |= FLAG_C;
    } else {
        state.f &= ~FLAG_C;
    }
    value = (value & 0x80) | (value >> 1);

    write_operand(state, instruction.operand1, op_code, value);
    return make_pair(value, 0);
}

pair<uint16_t, uint16_t> SRL(State& state, Instruction& instruction, vector<uint8_t>& op_code)
{
    uint8_t value = read_operand(state, instruction.operand1, op_code);

    if (value & 1) {
        state.f |= FLAG_C;
    } else {
        state.f &= ~FLAG_C;
    }
    value = value >> 1;

    write_operand(state, instruction.operand1, op_code, value);
    return make_pair(value, 0);
}

pair<uint16_t, uint16_t> SWAP(State& state, Instruction& instruction, vector<uint8_t>& op_code)
{
    uint8_t value = read_operand(state, instruction.operand1, op_code);
    value = (value & 0xf) << 4 | (value & 0xf0) >> 4;
    write_operand(state, instruction.operand1, op_code, value);
    return make_pair(value, 0);
}

pair<uint16_t, uint16_t> BIT(State& state, Instruction& instruction, vector<uint8_t>& op_code)
{
    uint8_t value = read_operand(state, instruction.operand2, op_code);
    uint8_t bit = 0;
    sscanf(instruction.operand1.c_str(), "%" SCNx8, &bit);
    if (value & (1 << bit)) {
	return make_pair(1, 0);
    } else {
	return make_pair(0, 0);
    }
}

pair<uint16_t, uint16_t> RES(State& state, Instruction& instruction, vector<uint8_t>& op_code)
{
    uint8_t value = read_operand(state, instruction.operand2, op_code);
    uint8_t bit = 0;
    sscanf(instruction.operand1.c_str(), "%" SCNx8, &bit);
    value &= ~(1 << bit);
    write_operand(state, instruction.operand2, op_code, value);
    return make_pair(0, 0);
}

pair<uint16_t, uint16_t> SET(State& state, Instruction& instruction, vector<uint8_t>& op_code)
{
    uint8_t value = read_operand(state, instruction.operand2, op_code);
    uint8_t bit = 0;
    sscanf(instruction.operand1.c_str(), "%" SCNx8, &bit);
    value |= 1 << bit;
    write_operand(state, instruction.operand2, op_code, value);
    return make_pair(0, 0);
}

pair<uint16_t, uint16_t> NOP(State& state, Instruction& instruction, vector<uint8_t>& op_code)
{
    return make_pair(0, 0);
}

pair<uint16_t, uint16_t> CCF(State& state, Instruction& instruction, vector<uint8_t>& op_code)
{
    if (state.f & FLAG_C) {
	state.f &= ~FLAG_C;
    } else {
	state.f |= FLAG_C;
    }
    return make_pair(0, 0);
}

pair<uint16_t, uint16_t> EI(State& state, Instruction& instruction, vector<uint8_t>& op_code)
{
    state.interrupts_enabled = true;
    return make_pair(0, 0);
}

pair<uint16_t, uint16_t> DI(State& state, Instruction& instruction, vector<uint8_t>& op_code)
{
    state.interrupts_enabled = false;
    return make_pair(0, 0);
}

pair<uint16_t, uint16_t> HALT(State& state, Instruction& instruction, vector<uint8_t>& op_code)
{
    state.halt_mode = true;
    return make_pair(0, 0);
}

pair<uint16_t, uint16_t> STOP(State& state, Instruction& instruction, vector<uint8_t>& op_code)
{
    if (state.prepare_double_speed) {
        state.prepare_double_speed = false;
        state.double_speed = true;
    } else {
        state.stop_mode = true;
    }
    return make_pair(0, 0);
}
