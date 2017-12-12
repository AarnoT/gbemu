#include "instruction.h"
#include "state.h"

#include <cstdint>
#include <utility>

const std::uint8_t FLAG_Z = 0x80;
const std::uint8_t FLAG_N = 0x40;
const std::uint8_t FLAG_H = 0x20;
const std::uint8_t FLAG_C = 0x10;

bool check_carry(std::pair<std::uint16_t, std::uint16_t> operands,
		 std::uint8_t carry_bit, std::uint8_t flags);
void update_flag(State& state, std::uint8_t flag_bit, FlagEffect& effect, bool value);
void update_flags(State& state, std::uint8_t* op_code,
		  std::pair<std::uint16_t, std::uint16_t> operands, bool uint16);

std::uint16_t uint8_to_uint16(std::uint8_t high, std::uint8_t low);
std::uint16_t read_register_pair(State& state, std::string& register_name);
void write_register_pair(State& state, std::string& register_name, std::uint16_t value);
std::uint8_t read_8bit_operand(State& state, const std::string& operand_name, std::uint8_t* code);

std::pair<std::uint16_t, std::uint16_t> LD(State& state, Instruction& instruction, std::uint8_t* code);
std::pair<std::uint16_t, std::uint16_t> LDH(State& state, Instruction& instruction, std::uint8_t* code);
std::pair<std::uint16_t, std::uint16_t> POP(State& state, Instruction& instruction, std::uint8_t* code);
std::pair<std::uint16_t, std::uint16_t> PUSH(State& state, Instruction& instruction, std::uint8_t* code);
std::pair<std::uint16_t, std::uint16_t> INC(State& state, Instruction& instruction, std::uint8_t* code);
std::pair<std::uint16_t, std::uint16_t> DEC(State& state, Instruction& instruction, std::uint8_t* code);
std::pair<std::uint16_t, std::uint16_t> DAA(State& state, Instruction& instruction, std::uint8_t* code);
std::pair<std::uint16_t, std::uint16_t> CPL(State& state, Instruction& instruction, std::uint8_t* code);
std::pair<std::uint16_t, std::uint16_t> ADD(State& state, Instruction& instruction, std::uint8_t* code);
std::pair<std::uint16_t, std::uint16_t> ADC(State& state, Instruction& instruction, std::uint8_t* code);
std::pair<std::uint16_t, std::uint16_t> SUB(State& state, Instruction& instruction, std::uint8_t* code);
std::pair<std::uint16_t, std::uint16_t> SBC(State& state, Instruction& instruction, std::uint8_t* code);
std::pair<std::uint16_t, std::uint16_t> AND(State& state, Instruction& instruction, std::uint8_t* code);
std::pair<std::uint16_t, std::uint16_t> XOR(State& state, Instruction& instruction, std::uint8_t* code);
std::pair<std::uint16_t, std::uint16_t> OR(State& state, Instruction& instruction, std::uint8_t* code);
std::pair<std::uint16_t, std::uint16_t> CP(State& state, Instruction& instruction, std::uint8_t* code);
