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


# Linkzeile von recvx-runtime.exe aus `ninja -t commands` des nativen MSVC-Builds
# (windows-arm64, Ninja, 2026-10-21), gekuerzt auf die Teilbefehle.
NINJA_MSVC = (
    r'C:\WINDOWS\system32\cmd.exe /C "cd . && "C:\Program Files\Microsoft Visual Studio\18\Insiders'
    r'\Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin\cmake.exe" -E vs_link_exe --msvc-ver=1952'
    r' --intdir=CMakeFiles\recvx-runtime.dir --rc=C:\PROGRA~2\WI3CF2~1\10\bin\100261~1.0\arm64\rc.exe'
    r' --mt=C:\PROGRA~2\WI3CF2~1\10\bin\100261~1.0\arm64\mt.exe --manifests  -- C:\PROGRA~1\MIB055~1\18'
    r'\Insiders\VC\Tools\MSVC\1452~1.370\bin\HOSTAR~1\arm64\link.exe /nologo'
    r' CMakeFiles\recvx-runtime.dir\native\runtime.cpp.obj CMakeFiles\recvx-runtime.dir\app-icon.rc.res'
    r'  /out:recvx-runtime.exe /implib:recvx-runtime.lib /pdb:recvx-runtime.pdb /version:0.0 /machine:ARM64'
    r' /INCREMENTAL:NO /subsystem:console  recvx_game.lib  kernel32.lib && C:\WINDOWS\system32\cmd.exe /C'
    r' "cd /D C:\Daten\recvx-native\build\windows-arm64-native && python.exe check_no_original_core.py'
    r' --binary C:/Daten/recvx-native/build/windows-arm64-native/recvx-runtime.exe""'
)


class MsvcLinkCommand(unittest.TestCase):
    """MSVC-Zweig ohne Werkzeugsatz: Befehlserkennung, Wrappermodell, Aufloesung, fail-closed."""

    def setUp(self):
        self.directory = Path(tempfile.mkdtemp())
        self.addCleanup(shutil.rmtree, self.directory, ignore_errors=True)

    def test_windows_split_keeps_backslashes_and_quoted_spaces(self):
        self.assertEqual(audit.windows_split(r'"C:\Program Files\a.exe" /out:x.exe C:\b\c.lib'),
                         [r'C:\Program Files\a.exe', '/out:x.exe', r'C:\b\c.lib'])
        self.assertEqual(audit.windows_split(r'a\\"b c" d\"e'), [r'a\b c', 'd"e'])
        with self.assertRaises(ValueError):
            audit.windows_split('"open')

    def test_link_command_is_the_wrapped_link_exe_with_its_manifest_option(self):
        argv = audit.msvc_link_from_commands(NINJA_MSVC, "recvx-runtime.exe", self.directory)
        self.assertEqual(Path(argv[0].replace("\\", "/")).name, "link.exe")
        self.assertIn("/out:recvx-runtime.exe", argv)
        self.assertIn("recvx_game.lib", argv)
        # cmVSLink, nicht inkrementell: der Wrapper haengt genau diese Option an.
        self.assertEqual(argv[-1], "/MANIFEST:EMBED,ID=1")
        self.assertNotIn("vs_link_exe", argv)
        self.assertNotIn("check_no_original_core.py", " ".join(argv))

    def test_other_target_and_posix_lines_are_not_msvc(self):
        self.assertIsNone(audit.msvc_link_from_commands(NINJA_MSVC, "recvx-assets.exe", self.directory))
        self.assertIsNone(audit.msvc_link_from_commands("zig-cxx main.o -o probe.exe", "probe.exe", self.directory))

    def test_manifest_no_and_incremental(self):
        line = NINJA_MSVC.replace("/INCREMENTAL:NO", "/INCREMENTAL:NO /MANIFEST:NO")
        argv = audit.msvc_link_from_commands(line, "recvx-runtime.exe", self.directory)
        self.assertFalse(any(token.startswith("/MANIFEST:EMBED") for token in argv))
        with self.assertRaises(RuntimeError):
            audit.msvc_link_from_commands(NINJA_MSVC.replace("/INCREMENTAL:NO", "/INCREMENTAL"),
                                          "recvx-runtime.exe", self.directory)

    def test_out_not_from_link_exe_fails(self):
        with self.assertRaises(RuntimeError):
            audit.msvc_link_from_commands("lld-link.exe /out:recvx-runtime.exe a.obj", "recvx-runtime.exe",
                                          self.directory)

    def test_response_file_is_expanded_and_missing_one_fails(self):
        (self.directory / "link.rsp").write_bytes("a.obj \"C:\\x y\\b.lib\"".encode("utf-16"))
        argv = audit.msvc_link_from_commands("link.exe /out:t.exe @link.rsp", "t.exe", self.directory)
        self.assertEqual(argv, ["link.exe", "/out:t.exe", "a.obj", r"C:\x y\b.lib"])
        with self.assertRaises(RuntimeError):
            audit.msvc_link_from_commands("link.exe /out:t.exe @absent.rsp", "t.exe", self.directory)

    def test_inputs_resolve_through_build_libpath_and_lib(self):
        sdk = self.directory / "sdk"
        extra = self.directory / "extra"
        sdk.mkdir()
        extra.mkdir()
        (self.directory / "own.lib").write_bytes(b"")
        (sdk / "kernel32.lib").write_bytes(b"")
        (extra / "special.lib").write_bytes(b"")
        argv = ["link.exe", f"/LIBPATH:{extra}", "own.lib", "kernel32.lib", "special.lib", "absent.lib",
                "app.res", "/out:t.exe"]
        inputs, missing = audit.msvc_explicit_inputs(argv, self.directory, {"LIB": str(sdk)})
        self.assertEqual([path.name for path in inputs], ["own.lib", "kernel32.lib", "special.lib"])
        self.assertEqual(missing, ["absent.lib"])

    def test_searched_libraries_are_parsed_and_localized_output_gives_none(self):
        english = ("Searching libraries\n    Searching C:\\SDK\\um\\arm64\\kernel32.lib:\n"
                   "    Searching C:\\VC\\lib\\arm64\\msvcrt.lib:\nFinished searching libraries\n")
        self.assertEqual([str(path).replace("/", "\\") for path in audit.searched_libraries(english)],
                         [r"C:\SDK\um\arm64\kernel32.lib", r"C:\VC\lib\arm64\msvcrt.lib"])
        self.assertEqual(audit.searched_libraries("    Durchsuchen von C:\\SDK\\kernel32.lib:\n"), [])

    def test_outputs_are_redirected_and_out_is_required(self):
        argv = ["link.exe", "/out:recvx-runtime.exe", "/implib:recvx-runtime.lib", "/pdb:sub\\r.pdb", "a.obj"]
        rebuilt = audit.msvc_redirect(argv, self.directory, "recvx-runtime.exe")
        self.assertEqual(rebuilt[1], f"/out:{self.directory / 'recvx-runtime.exe'}")
        self.assertEqual(rebuilt[2], f"/implib:{self.directory / 'recvx-runtime.lib'}")
        self.assertEqual(rebuilt[3], f"/pdb:{self.directory / 'r.pdb'}")
        self.assertEqual(rebuilt[4], "a.obj")
        with self.assertRaises(RuntimeError):
            audit.msvc_redirect(["link.exe", "a.obj"], self.directory, "t.exe")


if __name__ == "__main__":
    if not available():
        print("zig toolchain or llvm-nm missing; the Windows audit cannot be tested here")
        raise SystemExit(1)
    unittest.main(verbosity=1)
