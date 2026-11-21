#!/usr/bin/env python3
"""app-server-settings.py DIR: writes app_server's "appearance" and "fonts"
settings into DIR, as flattened BMessages (Renku's native '1FMH' format, see
headers/private/app/MessagePrivate.h); app_server reads them as it starts.

appearance: the composited desktop on, and the menu font's size.
fonts: the plain, bold and fixed fonts' sizes. Native apps scale their
controls with the plain font's size (12 being 1x), and the shell is drawn at
about two pixels a dp: 16 brings them closer while fixed-size windows, such
as the Appearance preferences, still fit the 720 pixel width (at 18 they
didn't). Set as files before app_server starts: changed live from the shell
as it started, input froze.
"""
import os
import struct
import sys

FONT_SIZE = 16.0
HASH_TABLE_SIZE = 5

B_BOOL_TYPE = 0x424F4F4C
B_FLOAT_TYPE = 0x464C4F54
B_STRING_TYPE = 0x43535452


def hash_name(name):
    # BMessage::_HashName
    result = 0
    for ch in name:
        result = ((result << 7) ^ (result >> 24)) & 0xFFFFFFFF
        result ^= ch
    return (result ^ (result << 12)) & 0xFFFFFFFF


def flatten(fields):
    """fields: (name, value) pairs; value a bool, a float or a str."""
    data = b""
    headers = []
    table = [-1] * HASH_TABLE_SIZE
    for index, (name, value) in enumerate(fields):
        name = name.encode()
        if isinstance(value, bool):
            type_code, item, fixed = B_BOOL_TYPE, bytes([value]), True
        elif isinstance(value, float):
            type_code, item, fixed = B_FLOAT_TYPE, struct.pack("<f", value), True
        else:
            # variable size: each item after its size
            text = value.encode() + b"\0"
            type_code, item, fixed = B_STRING_TYPE, struct.pack("<I", len(text)) + text, False
        # chained into its hash bucket (lookups follow the chain)
        bucket = hash_name(name) % HASH_TABLE_SIZE
        headers.append([0x0001 | (0x0002 if fixed else 0), len(name) + 1,
            type_code, 1, len(item), len(data), table[bucket]])
        table[bucket] = index
        data += name + b"\0" + item

    header = struct.pack("<4s2I3i3i3I5i",
        b"HMF1",            # MESSAGE_FORMAT_HAIKU, '1FMH' as little-endian int
        0,                  # what
        0x0001,             # MESSAGE_FLAG_VALID
        -1, -1, -1,         # target, current_specifier, message_area
        -1, -1, -1,         # reply_port, reply_target, reply_team
        len(data), len(fields), HASH_TABLE_SIZE,
        *table)
    # flags, name_length, type, count, data_size, offset, next_field
    return header + b"".join(struct.pack("<2H4Ii", *h) for h in headers) + data


out = sys.argv[1]
with open(os.path.join(out, "appearance"), "wb") as f:
    f.write(flatten([
        ("composited desktop", True),
        ("font family", "Noto Sans"),
        ("font style", "Regular"),
        ("font size", FONT_SIZE),
    ]))
with open(os.path.join(out, "fonts"), "wb") as f:
    f.write(flatten([
        ("plain family", "Noto Sans"), ("plain style", "Regular"),
        ("plain size", FONT_SIZE),
        ("bold family", "Noto Sans"), ("bold style", "Bold"),
        ("bold size", FONT_SIZE),
        ("fixed family", "Noto Sans Mono"), ("fixed style", "Regular"),
        ("fixed size", FONT_SIZE),
    ]))
