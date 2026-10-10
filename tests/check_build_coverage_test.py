"""Tests des Anschlusspruefers.

Die Faelle arbeiten auf eigenen Fixtures in einem temporaeren Baum, damit das
Ergebnis nicht vom jeweiligen Arbeitsstand des Projekts abhaengt. Zwei
Regressionsfaelle halten die beim Bau des Werkzeugs gefundenen Parserfehler
fest: eine ueber mehrere Zeilen laufende Deklaration und eine Deklaration als
erster Eintrag eines ``extern "C"``-Blocks wurden zuvor nicht erkannt, was
angeschlossene Quellen falsch als unangeschlossen gemeldet hat.
"""
import contextlib
import importlib.util
import io
import tempfile
import unittest
from pathlib import Path

spec = importlib.util.spec_from_file_location(
    'check_build_coverage',
    Path(__file__).resolve().parents[1] / 'tools/check_build_coverage.py')
coverage = importlib.util.module_from_spec(spec)
spec.loader.exec_module(coverage)


class CMakeParsingTests(unittest.TestCase):
    def test_multiline_target_keeps_every_source(self):
        targets = coverage.parse_targets(
            'add_executable(demo-tests tests/a_test.cpp\n'
            '               native/a.c)\n')
        self.assertEqual(targets['demo-tests']['sources'],
                         ['tests/a_test.cpp', 'native/a.c'])

    def test_target_sources_adds_to_the_same_target(self):
        targets = coverage.parse_targets(
            'add_library(lib native/a.c)\n'
            'target_sources(lib PRIVATE native/b.c)\n')
        self.assertEqual(sorted(targets['lib']['sources']),
                         ['native/a.c', 'native/b.c'])
        self.assertEqual(targets['lib']['kind'], 'add_library')

    def test_keywords_are_not_mistaken_for_sources(self):
        targets = coverage.parse_targets(
            'add_executable(tool EXCLUDE_FROM_ALL tools/t.cpp)\n')
        self.assertEqual(targets['tool']['sources'], ['tools/t.cpp'])
        self.assertTrue(targets['tool']['excluded'])

    def test_unresolved_variable_is_reported_not_guessed(self):
        targets = coverage.parse_targets('add_library(lib ${GENERATED_SOURCES})\n')
        self.assertEqual(targets['lib']['sources'], [])
        self.assertEqual(targets['lib']['unresolved'], ['${GENERATED_SOURCES}'])

    def test_comment_does_not_contribute_sources(self):
        targets = coverage.parse_targets(
            '# add_library(ghost native/ghost.c)\n'
            'add_library(real native/real.c)\n')
        self.assertNotIn('ghost', targets)
        self.assertEqual(targets['real']['sources'], ['native/real.c'])


class DeclarationParsingTests(unittest.TestCase):
    def test_multiline_declaration_is_recognised(self):
        """Regression: Deklaration ueber zwei Zeilen (vgl. native/collision.h)."""
        statements = coverage.declaration_statements(
            'float wall_ray_distance(const std::vector<Wall>& walls,\n'
            '                        float room_height);\n')
        self.assertEqual(len(statements), 1)
        self.assertRegex(statements[0], r'^float wall_ray_distance\(')

    def test_first_declaration_inside_extern_c_block(self):
        """Regression: der Linkage-Specifier verdeckte den ersten Eintrag."""
        names = []
        for statement in coverage.declaration_statements(
                '#ifdef __cplusplus\nextern "C" {\n#endif\n'
                'void bhEne20(ENEMY_WORK* w);\n'
                'void bhEne20_Init(ENEMY_WORK* w);\n'):
            match = coverage._DECL.match(statement)
            if match:
                names.append(match.group(1))
        self.assertEqual(names, ['bhEne20', 'bhEne20_Init'])

    def test_preprocessor_macro_is_not_an_entry_point(self):
        for statement in coverage.declaration_statements('#define scale(x) ((x)*2)\n'):
            self.assertIsNone(coverage._DECL.match(statement))

    def test_struct_field_is_not_an_entry_point(self):
        names = []
        for statement in coverage.declaration_statements(
                'typedef struct { int id; void (*step)(int); } WORK;\n'
                'void work_reset(WORK* w);\n'):
            match = coverage._DECL.match(statement)
            if match:
                names.append(match.group(1))
        self.assertEqual(names, ['work_reset'])


class AuditTests(unittest.TestCase):
    def setUp(self):
        self.temporary = tempfile.TemporaryDirectory(prefix='recvx coverage test ')
        self.addCleanup(self.temporary.cleanup)
        self.root = Path(self.temporary.name)
        (self.root / 'native').mkdir()
        (self.root / 'tests/native').mkdir(parents=True)

    def write(self, relative: str, text: str) -> None:
        (self.root / relative).write_text(text)

    def audit(self) -> dict:
        return coverage.audit(self.root, ['native', 'tests'])

    def test_source_outside_every_target_is_reported_as_not_built(self):
        self.write('CMakeLists.txt', 'add_library(game native/used.c)\n')
        self.write('native/used.c', 'void used(void){}\n')
        self.write('native/used.h', 'void used(void);\n')
        self.write('native/orphan.c', 'void orphan(void){}\nint more(void){return 0;}\n')
        self.write('native/orphan.h', 'void orphan(void);\n')
        findings = self.audit()['befunde']
        self.assertEqual([item['quelle'] for item in findings['nicht_gebaut']],
                         ['native/orphan.c'])
        orphan = findings['nicht_gebaut'][0]
        self.assertEqual(orphan['einstiegspunkte'], ['orphan'])
        self.assertEqual(orphan['zeilen'], 2)

    def test_built_source_without_any_caller_is_reported(self):
        self.write('CMakeLists.txt',
                   'add_library(game native/caller.c native/lonely.c)\n')
        self.write('native/caller.c', 'void caller(void){}\n')
        self.write('native/caller.h', 'void caller(void);\n')
        self.write('native/lonely.c', 'void lonely(void){}\n')
        self.write('native/lonely.h', 'void lonely(void);\n')
        findings = self.audit()['befunde']
        self.assertEqual([item['quelle'] for item in findings['gebaut_ohne_aufrufer']],
                         ['native/caller.c', 'native/lonely.c'])

    def test_caller_in_another_built_source_counts_as_connected(self):
        self.write('CMakeLists.txt',
                   'add_library(game native/host.c native/feature.c)\n')
        self.write('native/host.c', '#include "feature.h"\nvoid host(void){feature();}\n')
        self.write('native/feature.c', 'void feature(void){}\n')
        self.write('native/feature.h', 'void feature(void);\n')
        findings = self.audit()['befunde']
        self.assertNotIn('native/feature.c',
                         [item['quelle'] for item in findings['gebaut_ohne_aufrufer']])
        self.assertNotIn('native/feature.c',
                         [item['quelle'] for item in findings['nur_test']])

    def test_caller_only_in_tests_is_reported_separately(self):
        self.write('CMakeLists.txt',
                   'add_library(game native/feature.c)\n'
                   'add_executable(t tests/native/feature_test.cpp)\n')
        self.write('native/feature.c', 'void feature(void){}\n')
        self.write('native/feature.h', 'void feature(void);\n')
        self.write('tests/native/feature_test.cpp',
                   '#include "feature.h"\nint main(){feature();return 0;}\n')
        findings = self.audit()['befunde']
        self.assertEqual([item['quelle'] for item in findings['nur_test']],
                         ['native/feature.c'])

    def test_name_mentioned_without_call_is_separated_from_no_caller(self):
        """Ein Tabelleneintrag nennt den Namen ohne Klammer."""
        self.write('CMakeLists.txt',
                   'add_library(game native/table.c native/routine.c)\n')
        self.write('native/table.c',
                   '#include "routine.h"\nvoid (*tbl[1])(void) = { routine };\n')
        self.write('native/routine.c', 'void routine(void){}\n')
        self.write('native/routine.h', 'void routine(void);\n')
        findings = self.audit()['befunde']
        self.assertEqual([item['quelle'] for item in findings['nur_referenziert']],
                         ['native/routine.c'])
        self.assertEqual(findings['gebaut_ohne_aufrufer'], [])

    def test_source_without_header_makes_no_connection_claim(self):
        self.write('CMakeLists.txt', 'add_executable(main native/main.c)\n')
        self.write('native/main.c', 'int main(void){return 0;}\n')
        findings = self.audit()['befunde']
        self.assertEqual(findings['gebaut_ohne_aufrufer'], [])
        self.assertEqual(findings['nur_test'], [])

    def test_strict_mode_fails_only_when_a_finding_exists(self):
        # Befundfrei heisst: gebaut UND von einer anderen gebauten Quelle
        # aufgerufen. Ohne Aufrufer waere der Befund berechtigt.
        self.write('CMakeLists.txt', 'add_library(game native/used.c native/host.c)\n')
        self.write('native/used.c', 'void used(void){}\n')
        self.write('native/used.h', 'void used(void);\n')
        self.write('native/host.c', '#include "used.h"\nvoid host(void){used();}\n')
        with contextlib.redirect_stdout(io.StringIO()):
            clean = coverage.main(['--root', str(self.root), '--strict',
                                   '--directory', 'native'])
            self.write('native/orphan.c', 'void orphan(void){}\n')
            dirty = coverage.main(['--root', str(self.root), '--strict',
                                   '--directory', 'native'])
        self.assertEqual((clean, dirty), (0, 1))

    def test_report_and_json_both_render(self):
        self.write('CMakeLists.txt', 'add_library(game native/used.c)\n')
        self.write('native/used.c', 'void used(void){}\n')
        self.write('native/orphan.c', 'void orphan(void){}\n')
        result = self.audit()
        self.assertIn('nicht_gebaut', coverage.format_report(result))
        self.assertEqual(result['quellen_vorhanden'], 2)
        self.assertEqual(result['quellen_gebaut'], 1)


if __name__ == '__main__':
    unittest.main()
