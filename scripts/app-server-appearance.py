#!/usr/bin/env python3
"""app-server-appearance.py OUT: writes app_server's "appearance" settings
with the composited desktop on, as a flattened BMessage (Renku's native
'1FMH' format, see headers/private/app/MessagePrivate.h). app_server keeps its
defaults for everything the file doesn't set."""
import struct
import sys

NAME = b"composited desktop"
HASH_TABLE_SIZE = 5


def hash_name(name):
    # BMessage::_HashName
    result = 0
    for ch in name:
        result = ((result << 7) ^ (result >> 24)) & 0xFFFFFFFF
        result ^= ch
    return (result ^ (result << 12)) & 0xFFFFFFFF


name = NAME + b"\0"
value = b"\x01"
data = name + value

hash_table = [-1] * HASH_TABLE_SIZE
hash_table[hash_name(NAME) % HASH_TABLE_SIZE] = 0

header = struct.pack("<4s2I3i3i3I5i",
    b"HMF1",            # MESSAGE_FORMAT_HAIKU, '1FMH' as little-endian int
    0,                  # what
    0x0001,             # MESSAGE_FLAG_VALID
    -1, -1, -1,         # target, current_specifier, message_area
    -1, -1, -1,         # reply_port, reply_target, reply_team
    len(data), 1, HASH_TABLE_SIZE,
    *hash_table)
field = struct.pack("<2H4I i",
    0x0001 | 0x0002,    # FIELD_FLAG_VALID | FIELD_FLAG_FIXED_SIZE
    len(name),
    0x424F4F4C,         # B_BOOL_TYPE
    1,                  # count
    len(value),         # data_size, without the name
    0,                  # offset of the name in the data
    -1)                 # next_field

with open(sys.argv[1], "wb") as out:
    out.write(header + field + data)
