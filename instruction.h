#pragma once

#include <cstdint>
#include <string>
#include <vector>

enum FlagEffect {
    APPLY = 0,
    IGNORE,
    SET,
    CLEAR
};

typedef struct Instruction {
    std::string name;
    std::uint32_t bytes;
    std::uint32_t operand_count;
    std::string operand1;
    std::string operand2;
    std::uint32_t cycles;
    std::uint32_t branch_cycles;
    std::vector<FlagEffect> flags_ZNHC;
} Instruction;
