#!/usr/bin/env python3
"""Policy contracts; synthetic temp fixtures only, never read quarantine code."""
import importlib.util
import json
from pathlib import Path
import tempfile
import unittest
from unittest.mock import patch
from types import SimpleNamespace

ROOT = Path(__file__).resolve().parents[1]
spec = importlib.util.spec_from_file_location("native_policy", ROOT / "tools/check_no_original_core.py")
policy = importlib.util.module_from_spec(spec)
spec.loader.exec_module(policy)


class PolicyTest(unittest.TestCase):
    def setUp(self):
        self.temp = tempfile.TemporaryDirectory(prefix="recvx-policy-")
        self.root = Path(self.temp.name)
        (self.root / "native").mkdir()
        self.cmake = self.root / "CMakeLists.txt"
        self.cmake.write_text("add_library(recvx_math native/math.c)\n")
        (self.root / "native/math.c").write_text("float native_square(float n) { return n*n; }\n")

    def tearDown(self):
        self.temp.cleanup()

    def errors(self):
        return policy.audit(self.root)

    def test_clean_native(self):
        self.assertEqual(self.errors(), [])

    def test_original_no_data_whitelist(self):
        for source in ("item.c", "ps2_NaColi.c", "ps2_dvd_image.c", "ps2_NaMath.c"):
            with self.subTest(source=source):
                self.cmake.write_text(f'add_library(native_port "src/ps2/veronica/prog/{source}")\n')
                self.assertTrue(self.errors())

    def test_balanced_quotes_comments_and_nested(self):
        self.cmake.write_text('# add_library(recvx_original_engine x.c)\n#[=[ignored\nbody]=]\n'
                              'if(EXISTS "a(b)")\nadd_library(native_port native/math.c)\nendif()\n')
        self.assertEqual(self.errors(), [])

    def test_reference_dependency_is_not_compilation(self):
        self.cmake.write_text('add_custom_command(OUTPUT layout.h COMMAND python generate_types.py '
                              'DEPENDS src/ps2/veronica/prog/main.c)\n')
        self.assertEqual(self.errors(), [])

    def test_original_body_generator(self):
        self.cmake.write_text('add_custom_command(OUTPUT output.inc\nCOMMAND python generate_player_combat.py)\n')
        self.assertTrue(self.errors())

    def test_mixed_generator_is_not_data_only(self):
        # generate_native_tables.py lieferte ADX-Datenwerte, schrieb daneben
        # aber gesperrte Funktionsdateien. Ein Datenanteil macht einen
        # Erzeuger nicht zulaessig (R02).
        self.cmake.write_text('add_custom_command(OUTPUT adx_tables.inc\n'
                              'COMMAND python generate_native_tables.py)\n')
        self.assertTrue(self.errors())

    def test_generate_then_delete_is_not_a_safeguard(self):
        # Negativfall: ein Erzeuger schreibt kopierte Funktionsdateien und eine
        # nachgeschaltete Loeschung soll das heilen. Die Koerper entstehen
        # trotzdem; der Pfad bleibt gesperrt.
        self.cmake.write_text('add_custom_command(OUTPUT clean.inc\n'
                              'COMMAND python tools/some_tool.py\n'
                              'COMMAND cmake -E rm -f out/status_pulse.inc out/ui_tables.inc)\n')
        problems = self.errors()
        self.assertTrue(any("generates then deletes" in problem for problem in problems), problems)

    def test_pure_data_generator_allowed(self):
        # Klarstellung 2026-10-09: ein Generator, der ausschliesslich
        # Daten-/Layouttabellen ausgibt, ist zulaessig. Gesperrt bleiben
        # Funktionskoerper, Mischgeneratoren und Kernbindungen.
        self.cmake.write_text('add_custom_command(OUTPUT combat_tables.inc\n'
                              'COMMAND python tools/generate_combat_tables.py\n'
                              'DEPENDS src/ps2/veronica/prog/weapon.c)\n')
        self.assertEqual(self.errors(), [])

    def test_frozen_native_data_module_allowed(self):
        # Positivfall: eingefrorene Datenwerte als native Quelle im Baum, ohne
        # jeden Erzeugungsschritt beim Build.
        (self.root / "native/adx_tables.c").write_text('const short AdxVolTbl[2] = { 0, -2 };\n')
        self.cmake.write_text('add_library(recvx_game native/adx_tables.c)\n')
        self.assertEqual(self.errors(), [])

    def test_nested_cmake_not_a_bypass(self):
        directory = self.root / "native/component"
        directory.mkdir()
        (directory / "CMakeLists.txt").write_text('add_library(hidden src/ps2/veronica/prog/item.c)')
        self.assertTrue(self.errors())

    def test_quarantined_source_not_read(self):
        with patch.object(Path, "read_text", side_effect=AssertionError("must not read")):
            self.assertTrue(policy.audit(self.root / "archived/FORBIDDEN_DONTUSE"))

    def test_generator_command_after_dependency(self):
        self.cmake.write_text('add_custom_command(OUTPUT x.inc DEPENDS ref.c '
                              'COMMAND python generate_player_combat.py)')
        self.assertTrue(self.errors())

    def test_quarantine_inputs(self):
        for command in ('add_subdirectory(archived/FORBIDDEN_DONTUSE/x)',
                        'execute_process(COMMAND cp archived/FORBIDDEN_DONTUSE/x.c native/x.c)',
                        'file(GLOB files archived/FORBIDDEN_DONTUSE/*.c)'):
            self.cmake.write_text(command)
            self.assertTrue(self.errors())

    def test_hardware_and_dispatch(self):
        path = self.root / "native/math.c"
        for code in ('void x(){bhControlEnemy();}', 'void x(){recvx_vu0_vcallms(0);}',
                     'void x(){recvx_core_gs_execute(0);}', 'BH_PWORK ene[128];',
                     '#if 0\nvoid x(){recvx_original_init(0);}\n#endif'):
            path.write_text(code)
            self.assertTrue(self.errors())

    def test_body_include(self):
        (self.root / "native/math.c").write_text('#include "player_combat.inc"\n')
        self.assertTrue(self.errors())

    def test_native_data_include(self):
        (self.root / "native/math.c").write_text('#include "combat_tables.inc"\n')
        self.assertEqual(self.errors(), [])

    def test_transitive_compile(self):
        build = self.root / "build"
        build.mkdir()
        database = build / "compile_commands.json"
        database.write_text(json.dumps([{"file": "src/ps2/veronica/prog/item.c", "command": "cc -c item.c"}]))
        self.assertTrue(policy.check_build(build))

    def test_transitive_link(self):
        build = self.root / "build"
        (build / "CMakeFiles/game.dir").mkdir(parents=True)
        (build / "compile_commands.json").write_text("[]")
        (build / "CMakeFiles/game.dir/link.txt").write_text("c++ game.o librecvx_original_status.a")
        self.assertTrue(policy.check_build(build))

    def test_unknown_generated_function(self):
        build = self.root / "build"
        (build / "generated").mkdir(parents=True)
        (build / "compile_commands.json").write_text("[]")
        (build / "generated/renamed.h").write_text("static void renamed(){ return; }")
        self.assertTrue(policy.check_build(build))

    def test_missing_evidence_fails_closed(self):
        self.assertTrue(policy.check_build(self.root / "missing"))
        self.assertTrue(policy.check_binary(self.root / "missing-binary"))

    def test_binary_symbols_and_local_engine(self):
        binary = self.root / "runtime"
        binary.write_bytes(b"synthetic fixture, never executed")
        for symbols, rejected in (("000 T _native_enemy_tick\n", False),
                                  ("000 t _bhControlEnemy\n", True),
                                  ("000 B _sys\n", True),
                                  ("000 T _recvx_original_init\n", True),
                                  ("", True)):
            with self.subTest(symbols=symbols), patch.object(policy.subprocess, "run", return_value=SimpleNamespace(returncode=0, stdout=symbols)) as run:
                self.assertEqual(bool(policy.check_binary(binary, "synthetic-nm")), rejected)
                self.assertEqual(run.call_args.args[0], ["synthetic-nm", str(binary)])

    def test_quarantined_build_not_read(self):
        with patch.object(Path, "read_text", side_effect=AssertionError("must not read")):
            self.assertTrue(policy.check_build(self.root / "archived/FORBIDDEN_DONTUSE/build"))

    def test_no_symbol_tool_fails_closed(self):
        binary = self.root / "runtime"
        binary.write_bytes(b"synthetic")
        with patch.object(policy.shutil, "which", return_value=None):
            self.assertTrue(policy.check_binary(binary))

    def test_symbol_patterns(self):
        self.assertIsNotNone(policy.DISPATCH.fullmatch("bhJumpEnemy"))
        self.assertIsNotNone(policy.DISPATCH.fullmatch("core_enemy_world_create"))
        self.assertIsNone(policy.DISPATCH.fullmatch("native_enemy_tick"))


if __name__ == "__main__":
    unittest.main()
