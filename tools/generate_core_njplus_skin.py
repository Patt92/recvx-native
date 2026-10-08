#!/usr/bin/env python3
"""Extract the assembler-free skin-conversion routines of the original njplus.c.

npCnkFlatOff, npClrTranslate, npCopyMemory, npGetWHDSizeSub, npGetWHDSize,
npSkinConvPreparation, npSkinConvSub, npSkinConvMain, npSkinConvert,
npRetSkinConvMain and npRetSkinConvert are plain C in the original source.
They are emitted verbatim so the enemy/player closure binds the original
control flow instead of a host replacement.  npSetMemory/npSetMemoryL stay in
core_njplus_memory.c and are therefore not emitted here.
"""
from __future__ import annotations

import argparse
import re
from pathlib import Path

NAMES = (
    "npCnkFlatOff",
    "npClrTranslate",
    "npCopyMemory",
    "npGetWHDSizeSub",
    "npGetWHDSize",
    "npSkinConvPreparation",
    "npSkinConvSub",
    "npSkinConvMain",
    "npSkinConvert",
    "npRetSkinConvMain",
    "npRetSkinConvert",
)


def extract(source: str, name: str) -> str:
    """Return the original definition of `name`, unchanged."""
    pattern = re.compile(r"^[A-Za-z_][\w \*]*?[ \*]" + re.escape(name) + r"\s*\(", re.M)
    match = pattern.search(source)
    if not match:
        raise SystemExit("njplus.c: definition of %s not found" % name)
    brace = source.find("{", match.end())
    if brace < 0:
        raise SystemExit("njplus.c: body of %s not found" % name)
    depth = 0
    for i in range(brace, len(source)):
        if source[i] == "{":
            depth += 1
        elif source[i] == "}":
            depth -= 1
            if depth == 0:
                body = source[match.start():i + 1]
                if "asm" in body:
                    raise SystemExit("njplus.c: %s contains PS2 assembly" % name)
                return body
    raise SystemExit("njplus.c: unterminated body of %s" % name)


def main() -> None:
    parser = argparse.ArgumentParser()
    parser.add_argument("root", type=Path)
    parser.add_argument("output", type=Path)
    args = parser.parse_args()

    source = (args.root / "src/ps2/veronica/prog/njplus.c").read_text()
    parts = [extract(source, name) for name in NAMES]
    args.output.parent.mkdir(parents=True, exist_ok=True)
    args.output.write_text(
        "/* Generated from njplus.c; see generate_core_njplus_skin.py. */\n\n"
        + "\n\n".join(parts)
        + "\n")


if __name__ == "__main__":
    main()
