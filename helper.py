import json

with open('ops.json', 'r') as f:
    ops = json.loads(f.read())


def write_ops(f, op_dict):
    format_str = '{{{}, {}, {}, {}, {}, {}, {}, {}}},\n'
    for opcode, op in sorted(op_dict.items(), key=lambda i: int(i[0], base=16)):
        name = '"' + op['mnemonic'] + '"'
        op_bytes = op['bytes']
        operand_count = op['operand_count']
        operand1 = '"' + op.get('operand1', 'NONE') + '"'
        operand2 = '"' + op.get('operand2', 'NONE') + '"'
        cycles = op['cycles'][0]
        branch_cycles = op['cycles'][1] if len(op['cycles']) >= 2 else 0
        flag_dict = {'1' : 'FlagEffect::SET',
                     '0' : 'FlagEffect::CLEAR',
                     '-' : 'FlagEffect::IGNORE'}
        flags = [flag_dict.get(flag, 'FlagEffect::APPLY') for flag in op['flags_ZHNC']]
        flags = '{{{}}}'.format(', '.join(flags))
        f.write(format_str.format(
            name, op_bytes, operand_count, operand1, operand2, cycles,
            branch_cycles, flags))


with open('op_table.cpp', 'w') as f:
    f.write('#include "op_table.h"\n')
    f.write('#include "instruction.h"\n\n')
    f.write('#include <vector>\n\n')
    f.write('std::vector<Instruction> ops = {\n')
    write_ops(f, ops['unprefixed'])
    f.write('};\nstd::vector<Instruction> ops_cb = {\n')
    write_ops(f, ops['cbprefixed'])
    f.write('};\n')
