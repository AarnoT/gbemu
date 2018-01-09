#pragma once

#include "instruction.h"
#include "state.h"

#include <cstdint>
#include <map>
#include <utility>
#include <string>
#include <vector>

const std::uint8_t FLAG_Z = 0x80;
const std::uint8_t FLAG_N = 0x40;
const std::uint8_t FLAG_H = 0x20;
const std::uint8_t FLAG_C = 0x10;

typedef std::pair<std::uint16_t, std::uint16_t> (*OpFunction)(State&, Instruction&, std::vector<std::uint8_t>);
extern std::map<std::string, OpFunction> op_functions;

std::uint8_t execute_op(State& state);
bool address_executable(std::uint16_t addr);

bool check_carry(std::pair<std::uint16_t, std::uint16_t> operands,
		 std::uint8_t carry_bit, std::uint8_t flags);
void update_flag(State& state, std::uint8_t flag_bit, FlagEffect& effect, bool value);
void update_flags(State& state, std::vector<std::uint8_t> op_code,
		  std::pair<std::uint16_t, std::uint16_t> operands, bool uint16);

std::uint16_t uint8_to_uint16(std::uint8_t high, std::uint8_t low);
std::uint16_t read_register_pair(State& state, std::string& register_name);
void write_register_pair(State& state, std::string& register_name, std::uint16_t value);
std::uint16_t read_operand(State& state, const std::string& operand_name, std::vector<std::uint8_t> op_code);
void write_operand(State& state, const std::string& operand_name, std::vector<std::uint8_t> op_code, std::uint16_t value);
bool check_condition(State& state, std::string& condition_code);
uint16_t pop_from_stack(State& state);
void push_onto_stack(State& state, uint16_t value);

std::pair<std::uint16_t, std::uint16_t> LD(State& state, Instruction& instruction, std::vector<std::uint8_t> op_code);
std::pair<std::uint16_t, std::uint16_t> POP(State& state, Instruction& instruction, std::vector<std::uint8_t> op_code);
std::pair<std::uint16_t, std::uint16_t> PUSH(State& state, Instruction& instruction, std::vector<std::uint8_t> op_code);
std::pair<std::uint16_t, std::uint16_t> INC(State& state, Instruction& instruction, std::vector<std::uint8_t> op_code);
std::pair<std::uint16_t, std::uint16_t> DEC(State& state, Instruction& instruction, std::vector<std::uint8_t> op_code);
std::pair<std::uint16_t, std::uint16_t> DAA(State& state, Instruction& instruction, std::vector<std::uint8_t> op_code);
std::pair<std::uint16_t, std::uint16_t> CPL(State& state, Instruction& instruction, std::vector<std::uint8_t> op_code);
std::pair<std::uint16_t, std::uint16_t> ADD(State& state, Instruction& instruction, std::vector<std::uint8_t> op_code);
std::pair<std::uint16_t, std::uint16_t> ADC(State& state, Instruction& instruction, std::vector<std::uint8_t> op_code);
std::pair<std::uint16_t, std::uint16_t> SUB(State& state, Instruction& instruction, std::vector<std::uint8_t> op_code);
std::pair<std::uint16_t, std::uint16_t> SBC(State& state, Instruction& instruction, std::vector<std::uint8_t> op_code);
std::pair<std::uint16_t, std::uint16_t> AND(State& state, Instruction& instruction, std::vector<std::uint8_t> op_code);
std::pair<std::uint16_t, std::uint16_t> XOR(State& state, Instruction& instruction, std::vector<std::uint8_t> op_code);
std::pair<std::uint16_t, std::uint16_t> OR(State& state, Instruction& instruction, std::vector<std::uint8_t> op_code);
std::pair<std::uint16_t, std::uint16_t> CP(State& state, Instruction& instruction, std::vector<std::uint8_t> op_code);
std::pair<std::uint16_t, std::uint16_t> JR(State& state, Instruction& instruction, std::vector<std::uint8_t> op_code);
std::pair<std::uint16_t, std::uint16_t> JP(State& state, Instruction& instruction, std::vector<std::uint8_t> op_code);
std::pair<std::uint16_t, std::uint16_t> RET(State& state, Instruction& instruction, std::vector<std::uint8_t> op_code);
std::pair<std::uint16_t, std::uint16_t> RETI(State& state, Instruction& instruction, std::vector<std::uint8_t> op_code);
std::pair<std::uint16_t, std::uint16_t> CALL(State& state, Instruction& instruction, std::vector<std::uint8_t> op_code);
std::pair<std::uint16_t, std::uint16_t> RST(State& state, Instruction& instruction, std::vector<std::uint8_t> op_code);
std::pair<std::uint16_t, std::uint16_t> RLCA(State& state, Instruction& instruction, std::vector<std::uint8_t> op_code);
std::pair<std::uint16_t, std::uint16_t> RLA(State& state, Instruction& instruction, std::vector<std::uint8_t> op_code);
std::pair<std::uint16_t, std::uint16_t> RRCA(State& state, Instruction& instruction, std::vector<std::uint8_t> op_code);
std::pair<std::uint16_t, std::uint16_t> RRA(State& state, Instruction& instruction, std::vector<std::uint8_t> op_code);
std::pair<std::uint16_t, std::uint16_t> RLC(State& state, Instruction& instruction, std::vector<std::uint8_t> op_code);
std::pair<std::uint16_t, std::uint16_t> RRC(State& state, Instruction& instruction, std::vector<std::uint8_t> op_code);
std::pair<std::uint16_t, std::uint16_t> RL(State& state, Instruction& instruction, std::vector<std::uint8_t> op_code);
std::pair<std::uint16_t, std::uint16_t> RR(State& state, Instruction& instruction, std::vector<std::uint8_t> op_code);
std::pair<std::uint16_t, std::uint16_t> SLA(State& state, Instruction& instruction, std::vector<std::uint8_t> op_code);
std::pair<std::uint16_t, std::uint16_t> SRA(State& state, Instruction& instruction, std::vector<std::uint8_t> op_code);
std::pair<std::uint16_t, std::uint16_t> SRL(State& state, Instruction& instruction, std::vector<std::uint8_t> op_code);
std::pair<std::uint16_t, std::uint16_t> SWAP(State& state, Instruction& instruction, std::vector<std::uint8_t> op_code);
std::pair<std::uint16_t, std::uint16_t> BIT(State& state, Instruction& instruction, std::vector<std::uint8_t> op_code);
std::pair<std::uint16_t, std::uint16_t> RES(State& state, Instruction& instruction, std::vector<std::uint8_t> op_code);
std::pair<std::uint16_t, std::uint16_t> SET(State& state, Instruction& instruction, std::vector<std::uint8_t> op_code);
std::pair<std::uint16_t, std::uint16_t> NOP(State& state, Instruction& instruction, std::vector<std::uint8_t> op_code);
std::pair<std::uint16_t, std::uint16_t> CCF(State& state, Instruction& instruction, std::vector<std::uint8_t> op_code);
std::pair<std::uint16_t, std::uint16_t> EI(State& state, Instruction& instruction, std::vector<std::uint8_t> op_code);
std::pair<std::uint16_t, std::uint16_t> DI(State& state, Instruction& instruction, std::vector<std::uint8_t> op_code);
