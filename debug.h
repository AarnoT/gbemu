#pragma once

#include "instruction.h"
#include "state.h"

#include <cstdint>
#include <deque>
#include <vector>

extern std::deque<std::uint16_t> recent_jumps;

void add_jump(std::uint16_t addr);

void print_debug_info(State& state, Instruction& instruction, std::vector<std::uint8_t> op_code);
