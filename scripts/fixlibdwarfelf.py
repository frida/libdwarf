#!/usr/bin/env python3

import sys


def fix_libdwarf_elf(input_path, output_path):
    with open(input_path, 'r', encoding='utf-8') as input_file, \
            open(output_path, 'w', encoding='utf-8') as output_file:
        for line in input_file:
            if line == "#define _LIBDWARF_H\n":
                output_file.write(line)
                output_file.write("\n#include <libelf.h>\n\n")
                continue

            if line == "typedef struct Elf Elf;\n":
                continue

            output_file.write(line)


if __name__ == '__main__':
    input_path = sys.argv[1]
    output_path = sys.argv[2]
    try:
        fix_libdwarf_elf(input_path, output_path)
    except Exception as e:
        print("Unable to fixup libdwarf header:", e, file=sys.stderr)
        sys.exit(1)
