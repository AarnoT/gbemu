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
