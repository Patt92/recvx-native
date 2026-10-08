#!/usr/bin/env python3
"""Extract the assembler-free collision prefix of the original njplus.c.

The source through npDistanceP2C is ordinary C and is the exact control flow
used by enemy hit tests.  Later njplus routines contain PS2 VU assembly and
are deliberately not emitted here: they need their own instruction-level host
port, rather than a fallback implementation.
"""
from __future__ import annotations

import argparse
from pathlib import Path


END = "// 100% matching!\nvoid npDrawPlane"


def main() -> None:
    parser = argparse.ArgumentParser()
    parser.add_argument("root", type=Path)
    parser.add_argument("output", type=Path)
    args = parser.parse_args()

    source = (args.root / "src/ps2/veronica/prog/njplus.c").read_text()
    end = source.find(END)
    if end < 0:
        raise SystemExit("njplus collision boundary not found")
    extracted = source[:end]
    required = ("npCollisionCheckCC", "npCollisionCheckCCEx", "npCollisionCheckSC", "npDistanceP2C")
    if any(name not in extracted for name in required):
        raise SystemExit("njplus collision functions missing from extracted range")
    args.output.parent.mkdir(parents=True, exist_ok=True)
    args.output.write_text("/* Generated from njplus.c; see generate_core_njplus.py. */\n" + extracted)


if __name__ == "__main__":
    main()
