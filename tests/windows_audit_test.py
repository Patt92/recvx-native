#!/usr/bin/env python3
"""Negativpruefungen des Windows-PE-Audits (tools/windows_link_audit.py).

Geprueft wird, dass der Nachweis scheitert, wenn eine Bibliothek fehlt, ein
Archivmitglied ein verbotenes Symbol traegt oder Code-/Datenbytes des
auszuliefernden Binaries manipuliert sind. Die Tests brauchen den
Zig-Werkzeugsatz und llvm-nm; ohne sie schlagen sie fehl, statt zu schweigen.
"""
import os
import shutil
import struct
import subprocess
import sys
import tempfile
import unittest
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / "tools"))
import windows_link_audit as audit  # noqa: E402
from check_no_original_core import forbidden_symbols  # noqa: E402

ZIG_CXX = ROOT / "build/toolchains/zig-cxx"
ZIG_AR = ROOT / "build/toolchains/zig-ar"
CACHE = ROOT / "build/toolchains/zig-cache"
NM = shutil.which("llvm-nm") or "/opt/homebrew/opt/llvm/bin/llvm-nm"


def available():
    return ZIG_CXX.is_file() and ZIG_AR.is_file() and Path(NM).is_file()


def zig_env():
    return dict(os.environ, ZIG_GLOBAL_CACHE_DIR=str(CACHE))


@unittest.skipUnless(available(), "zig toolchain or llvm-nm missing")
class WindowsAudit(unittest.TestCase):
    def setUp(self):
        self.directory = Path(tempfile.mkdtemp())
        self.addCleanup(shutil.rmtree, self.directory, ignore_errors=True)
        (self.directory / "main.cpp").write_text(
            "int helper(int x){return x*3;}\nint main(){return helper(2);}\n")
        self.exe = self.directory / "probe.exe"
        self.command = [str(ZIG_CXX), "-O3", "main.cpp", "-o", str(self.exe)]
        done = subprocess.run(self.command, cwd=self.directory, capture_output=True,
                              text=True, env=zig_env())
        self.assertEqual(done.returncode, 0, done.stderr[-400:])

    # --- Stufe 3 ---------------------------------------------------------
    def test_identical_relink_passes(self):
        rebuilt = audit.comparison_link(self.command, self.directory, self.exe, zig_env())
        self.assertEqual(audit.compare_images(self.exe.read_bytes(), rebuilt), [])

    def test_tampered_code_byte_fails(self):
        shipped = bytearray(self.exe.read_bytes())
        rebuilt = audit.comparison_link(self.command, self.directory, self.exe, zig_env())
        # Erstes Byte des Codeabschnitts kippen: das liegt in keinem
        # Metadatenfeld und muss auffallen.
        lfanew = struct.unpack_from("<I", shipped, 0x3C)[0]
        coff = lfanew + 4
        sections = struct.unpack_from("<H", shipped, coff + 2)[0]
        optional_size = struct.unpack_from("<H", shipped, coff + 16)[0]
        table = coff + 20 + optional_size
        text = None
        for index in range(sections):
            entry = table + index * 40
            if shipped[entry:entry + 8].rstrip(b"\0") == b".text":
                text = struct.unpack_from("<I", shipped, entry + 20)[0]
        self.assertIsNotNone(text, "probe has no .text section")
        shipped[text] ^= 0xFF
        problems = audit.compare_images(bytes(shipped), rebuilt)
        self.assertTrue(problems, "a flipped code byte passed the comparison")
        self.assertIn("outside the parsed metadata fields", problems[0])

    def test_tampered_data_byte_fails(self):
        shipped = bytearray(self.exe.read_bytes())
        rebuilt = audit.comparison_link(self.command, self.directory, self.exe, zig_env())
        shipped[len(shipped) - 1] ^= 0xFF
        self.assertTrue(audit.compare_images(bytes(shipped), rebuilt),
                        "a flipped trailing data byte passed the comparison")

    def test_metadata_fields_are_parsed_not_ranges(self):
        fields = audit.pe_metadata_fields(self.exe.read_bytes())
        names = [name for name, _, _ in fields]
        self.assertIn("COFF TimeDateStamp", names)
        # Kein Feld darf einen ganzen Abschnitt abdecken.
        for name, _, length in fields:
            self.assertLessEqual(length, 20, f"{name} covers {length} bytes")

    # --- Stufe 1/2 -------------------------------------------------------
    def test_missing_library_fails(self):
        library = self.directory / "libabsent.a"
        argv = self.command + [str(library)]
        inputs, missing = audit.explicit_inputs(argv, self.directory)
        self.assertEqual(missing, [str(library)])
        self.assertNotIn(library.resolve(), inputs)

    def test_forbidden_archive_member_fails(self):
        # Bewusst mit dem Hostcompiler und ohne den Zig-Cache: ein verbotenes
        # Objekt darf nicht in den Werkzeugsatz-Cache gelangen. Die Pruefung
        # arbeitet ueber Symbolnamen und ist nicht zielplattformgebunden.
        host = shutil.which("cc") or shutil.which("clang")
        archiver = shutil.which("llvm-ar") or shutil.which("ar")
        self.assertTrue(host and archiver, "no host compiler/archiver for the fixture")
        (self.directory / "member.c").write_text("int sys;\nint helper_two(void){return 1;}\n")
        member = self.directory / "member.o"
        done = subprocess.run([host, "-c", "member.c", "-o", str(member)],
                              cwd=self.directory, capture_output=True, text=True)
        self.assertEqual(done.returncode, 0, done.stderr[-400:])
        archive = self.directory / "libmember.a"
        done = subprocess.run([archiver, "qc", str(archive), str(member)],
                              cwd=self.directory, capture_output=True, text=True)
        self.assertEqual(done.returncode, 0, done.stderr[-400:])
        problems = audit.audit_inputs([archive], NM, forbidden_symbols)
        self.assertTrue(problems, "a forbidden archive member passed the input audit")
        self.assertIn("sys", problems[0])

    def test_clean_inputs_pass(self):
        host = shutil.which("cc") or shutil.which("clang")
        self.assertTrue(host, "no host compiler for the fixture")
        clean = self.directory / "clean.o"
        done = subprocess.run([host, "-c", "main.cpp", "-o", str(clean), "-x", "c++"],
                              cwd=self.directory, capture_output=True, text=True)
        self.assertEqual(done.returncode, 0, done.stderr[-400:])
        self.assertEqual(audit.audit_inputs([clean], NM, forbidden_symbols), [])

    def test_unreadable_archive_fails(self):
        broken = self.directory / "libbroken.a"
        broken.write_bytes(b"not an archive")
        self.assertTrue(audit.archives_readable([broken], NM),
                        "an unreadable archive passed as evidence")

    def test_missing_toolchain_cache_fails(self):
        with self.assertRaises(RuntimeError):
            audit.implicit_inputs(self.directory / "absent-cache")


if __name__ == "__main__":
    if not available():
        print("zig toolchain or llvm-nm missing; the Windows audit cannot be tested here")
        raise SystemExit(1)
    unittest.main(verbosity=1)
