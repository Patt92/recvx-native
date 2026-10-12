#!/usr/bin/env python3
"""Read-only F05 evidence audit against the original ELF; executes no original code."""
import argparse
import json
import struct
from pathlib import Path


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("elf", type=Path)
    args = parser.parse_args()
    data = args.elf.read_bytes()
    if data[:6] != b"\x7fELF\x01\x01":
        raise ValueError("expected little-endian ELF32")
    phoff = struct.unpack_from("<I", data, 28)[0]
    phsize, phcount = struct.unpack_from("<HH", data, 42)
    segments = []
    for i in range(phcount):
        kind, offset, address, _, length = struct.unpack_from(
            "<IIIII", data, phoff + i * phsize)
        if kind == 1:
            segments.append((address, offset, length))

    def read(address, length):
        for base, offset, size in segments:
            if base <= address and address + length <= base + size:
                start = offset + address - base
                return data[start:start + length]
        raise ValueError(f"unmapped ELF address {address:#x}")

    # Instruction words independently recorded from disasm_ps2.py, not native
    # implementation output. This audit deliberately fails on a different ELF.
    instructions = {
        # CalcPs2ZbuffAB: signed clip loads, numerator order, divide, stores.
        0x2DF1F4: 0xC4229C60, 0x2DF1F8: 0x3C03C780,
        0x2DF200: 0x3C034780, 0x2DF20C: 0xC4239C68,
        0x2DF210: 0x46020002, 0x2DF214: 0x46020842,
        0x2DF21C: 0x46021881, 0x2DF220: 0x46030002,
        0x2DF224: 0x46020843, 0x2DF228: 0x46020003,
        0x2DF22C: 0xE4212F48, 0x2DF23C: 0xE4202F40,
        # CvVMod: 1/z; no sign inversion before store at vertex +0x38.
        0x2D1DEC: 0x3C023F80, 0x2D1DF0: 0x44820000,
        0x2D1DF8: 0x46140083, 0x2D1E30: 0xE6420038,
        # Mod coefficient negation and actual PACKED tag base.
        0x2CC658: 0xC4212F40, 0x2CC668: 0xC4202F48,
        0x2CC66C: 0x46000847, 0x2CC670: 0xE7A10050,
        0x2CC674: 0x46000007, 0x2CC678: 0xE7A00058,
        0x2CC6E4: 0x3C033000, 0x2CC6E8: 0x34634000,
        0x2CC6EC: 0x0003203C, 0x2CC6F0: 0x34038000,
        0x2CC6F4: 0x00641825, 0x2CC72C: 0x24020412,
        # Guard ACC is overwritten at 7AC; VMADDX writes destination only.
        0x2CC79C: 0x4A465ABC, 0x2CC7A0: 0x4A4B31C8,
        0x2CC7A8: 0x4BCD39FF, 0x2CC7AC: 0x4A4052BC,
        0x2CC7B0: 0x4A4A2188, 0x2CC7B4: 0x4A4031AB,
        0x2CC7BC: 0x4A4C3195, 0x2CC7C4: 0x4BE6317D,
        0x2CC7D0: 0x00641824, 0x2CC7DC: 0x4A0218F5,
    }
    for address, expected in instructions.items():
        actual = struct.unpack("<I", read(address, 4))[0]
        if actual != expected:
            raise ValueError(f"ELF {address:#x}: {actual:#010x} != {expected:#010x}")
    expected_vectors = {
        0x35E100: (2048.0, 2048.0, 0.0, 2047.0),
        0x35E110: (1.0, 65534.0, 0.0, 0.0),
        0x35E120: (0.06250190734863281, 0.0, -2048.0625, 0.0),
    }
    for address, expected in expected_vectors.items():
        if struct.unpack("<4f", read(address, 16)) != expected:
            raise ValueError(f"ELF guard constants differ at {address:#x}")
    tag_base = 0x3000400000008000
    print(json.dumps({
        "instruction_words_verified": len(instructions),
        "constant_vectors_verified": len(expected_vectors),
        "reciprocal_z": "positive 1/view_z",
        "depth": "clamp(-a-b*q,0,65534), vftoi4, XYZF2 lane bits 4..27",
        "accumulator": "depth VADDA replaces guard ACC.z; VMADDX writes vertex",
        "packet": "PACKED" if (tag_base >> 58) & 3 == 0 else "unexpected",
        "scope": "static ELF evidence only; no renderer/gameplay acceptance",
    }, indent=2))


if __name__ == "__main__":
    main()
