#!/usr/bin/env python3
import re
import sys

# Parses a common style of defining enums in c, printing each with their hex value.
# With small modifications, this could be used to generate a function to convert status codes to strings.
# 
# To parse the statuses in this repo
# cat src/include/com/amazonaws/kinesis/video/webrtcclient/Include.h | ./scripts/parse_status.py 

'''
# Example usage (uncomment as a basic test)
paragraph="""
#define STATUS_PARENT 0x4
#define STATUS_CHILD STATUS_PARENT + 0x1
"""
operands_map = operands_by_name(paragraph) 
print(operands_map) # {'STATUS_CHILD': ('STATUS_PARENT', 1), 'STATUS_PARENT': (None, 4)}
example_sums = hex_sums(operands_map)
print(example_sums) # {'STATUS_CHILD': "0x5", 'STATUS_PARENT': "0x4"}
'''

pattern = re.compile("#define *(STATUS\_[A-Z_]*) *(([A-Z_]*) *\+ *)?0x([0-9a-fA-F]*)")

def operands_by_name(paragraph):
    matches = filter(None, [pattern.match(line) for line in paragraph.splitlines()])
    return {groups[0]: (groups[2], int(groups[3], base=16)) for groups in 
            [match.groups() for match in matches]}

def sum_value(by_name, name):
    base, idx = by_name[name]
    return idx if base is None else idx + sum_value(by_name, base)

def hex_sums(by_name):
    return {name: hex(sum_value(operands_map, name)) for name in by_name.keys()}

paragraph = sys.stdin.read()
operands_map = operands_by_name(paragraph)
sums_map = hex_sums(operands_map)
longest_status = len(max(sums_map.keys(), key=len))
lines = ["{:{}s} {}".format(name, longest_status, value) for name, value in sums_map.items()]
print("\n".join(lines))

