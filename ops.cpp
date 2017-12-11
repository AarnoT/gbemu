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
        state.f = num1; 
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
