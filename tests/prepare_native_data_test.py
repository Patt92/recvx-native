"""--iso pipeline of prepare_native_data.py: one synthetic ISO, fixture exporters.

The exporters are small scripts with the real command-line contract; the
movie step is replaced by a fixture because it needs real PSS streams.
"""
import importlib.util
import json
from pathlib import Path
import struct
import sys
import tempfile
import unittest
from unittest.mock import patch

TESTS = Path(__file__).resolve().parent
sys.path.insert(0, str(TESTS))
import iso9660_test

spec = importlib.util.spec_from_file_location('prepare_native_data', TESTS.parent / 'tools/prepare_native_data.py')
prepare = importlib.util.module_from_spec(spec)
spec.loader.exec_module(prepare)
import host_tools
import cook_movies


class HostToolTests(unittest.TestCase):
    def setUp(self):
        self.temporary = tempfile.TemporaryDirectory(prefix='recvx-host-tools-')
        self.addCleanup(self.temporary.cleanup)
        self.root = Path(self.temporary.name)
        (self.root / 'build-tools').mkdir()
        self.local = self.root / 'build-tools/ffmpeg.exe'
        self.local.touch()
        self.addCleanup(patch.stopall)
        patch.object(host_tools.platform, 'system', return_value='Windows').start()
        self.which = patch.object(host_tools.shutil, 'which', return_value=None).start()

    def test_windows_fallback_for_bare_name_and_exe(self):
        for name in ('ffmpeg', 'ffmpeg.exe'):
            self.assertEqual(host_tools.find(name, self.root), str(self.local.resolve()))

    def test_path_takes_precedence(self):
        self.which.return_value = 'C:/installed/ffmpeg.exe'
        self.assertEqual(host_tools.find('ffmpeg', self.root), self.which.return_value)

    def test_explicit_path_is_preserved(self):
        self.which.return_value = 'C:/custom/ffmpeg.exe'
        self.assertEqual(host_tools.find('C:/custom/ffmpeg.exe', self.root), self.which.return_value)

    def test_missing_explicit_path_does_not_fall_back(self):
        self.assertIsNone(host_tools.find('C:/missing/ffmpeg.exe', self.root))
        self.assertIsNone(host_tools.find('./ffmpeg.exe', self.root))

    def test_missing_tool_returns_none(self):
        self.assertIsNone(host_tools.find('ffprobe', self.root))

    def test_other_platforms_do_not_use_windows_executable(self):
        for system in ('Darwin', 'Linux'):
            with patch.object(host_tools.platform, 'system', return_value=system):
                self.assertIsNone(host_tools.find('ffmpeg', self.root))

    def test_direct_movie_command_passes_resolved_tools(self):
        data = self.root / 'data'
        (data / 'files/MOVIE').mkdir(parents=True)
        (data / 'files/MOVIE/MV_0000.PSS').touch()
        (data / 'movies').mkdir()
        tools = [str(self.local.resolve()), str(self.root / 'build-tools/ffprobe.exe')]
        with patch.object(sys, 'argv', ['cook_movies.py', '--data', str(data)]), \
             patch.object(cook_movies, 'find_host_tool', side_effect=tools), \
             patch.object(cook_movies, 'cook', return_value={'frames': 1, 'seconds': 1.0}) as cook:
            cook_movies.main()
        self.assertEqual(cook.call_args.args[2:], tuple(tools))

EXPORTER = '''#!/usr/bin/env python3
import struct, sys
from pathlib import Path
source, output = Path(sys.argv[1]), Path(sys.argv[2])
if output.exists():
    sys.exit('export destination already exists')
for path in sorted(source.rglob('*')):
    if not path.is_file():
        continue
    if path.suffix.upper() == '.AFS':
        data = path.read_bytes()
        count = struct.unpack_from('<I', data, 4)[0]
        for i in range(count):
            offset, size = struct.unpack_from('<II', data, 8 + i * 8)
            target = output / 'archives' / path.stem.upper() / f'{i:04}.bin'
            target.parent.mkdir(parents=True, exist_ok=True)
            target.write_bytes(data[offset:offset + size])
    else:
        target = output / 'files' / path.relative_to(source)
        target.parent.mkdir(parents=True, exist_ok=True)
        target.write_bytes(path.read_bytes())
(output / 'rooms').mkdir()
for name in ('RM_0000', 'RM_0010'):
    (output / 'rooms' / f'{name}.RDT').write_bytes(name.encode())
(output / 'native-data.version').write_text('recvx-assets-1\\n')
'''

WORLD = '''#!/usr/bin/env python3
import sys
from pathlib import Path
data, room = Path(sys.argv[1]), sys.argv[2]
if not (data / 'rooms' / f'{room}.RDT').is_file():
    sys.exit('missing room')
target = data / 'textures/npcs' / room / '0000/0000.tga'
if target.exists():
    sys.exit('asset output already exists')
target.parent.mkdir(parents=True)
target.write_bytes(room.encode())
print('Exported 1 room actor/object/item textures.')
'''

FAILING_WORLD = '''#!/usr/bin/env python3
import sys
if sys.argv[2] == 'RM_0010':
    sys.exit('fixture decode failure')
print('Exported 0 room actor/object/item textures.')
'''


def afs(entries):
    header = struct.pack('<II', 0x00534641, len(entries))
    index = b''
    blobs = b''
    for entry in entries:
        index += struct.pack('<II', 8 + 8 * len(entries) + len(blobs), len(entry))
        blobs += entry
    return header + index + blobs


class IsoPipelineTests(unittest.TestCase):
    def setUp(self):
        self.temporary = tempfile.TemporaryDirectory(prefix='recvx-prepare-test-')
        self.addCleanup(self.temporary.cleanup)
        self.root = Path(self.temporary.name)
        self.tools = self.root / 'bin'
        self.tools.mkdir()
        self.exporter = self.script('recvx-assets', EXPORTER)
        self.world = self.script('recvx-world-assets', WORLD)
        self.image = iso9660_test.build_iso(self.root / 'game.iso', system=afs([b'ONE', b'TWO']))
        self.output = self.root / 'data'
        self.movies = []
        self.addCleanup(patch.stopall)
        patch.object(prepare, 'ROOT', self.root).start()
        patch.object(prepare, 'cook_movies', side_effect=self.cook_movies).start()

    def script(self, name, text):
        path = self.tools / name
        path.write_text(text)
        path.chmod(0o755)
        return path

    def cook_movies(self, root, ffmpeg, ffprobe):
        self.movies.append((Path(ffmpeg).name, Path(ffprobe).name))
        self.assertTrue((root / 'textures/npcs/RM_0010/0000/0000.tga').is_file(), 'movies must follow world textures')
        (root / 'movies').mkdir()
        (root / 'movies/0000.rvv').write_bytes(b'RVV1 fixture')

    def run_pipeline(self, ffmpeg=sys.executable):
        prepare.export_iso([self.image], self.output, self.exporter, ffmpeg, sys.executable, 2)

    def staging_left(self):
        return [p for p in (self.root / 'build').iterdir()] if (self.root / 'build').exists() else []

    def test_complete_tree_is_published_with_manifest(self):
        self.run_pipeline()
        self.assertEqual((self.output / 'archives/SYSTEM/0000.bin').read_bytes(), b'ONE')
        self.assertEqual((self.output / 'archives/SYSTEM/0001.bin').read_bytes(), b'TWO')
        self.assertEqual((self.output / 'files/SUB/INNER.BIN').read_bytes(), b'INNER!')
        for room in ('RM_0000', 'RM_0010'):
            self.assertEqual((self.output / 'textures/npcs' / room / '0000/0000.tga').read_bytes(), room.encode())
        self.assertEqual(len(self.movies), 1)
        self.assertTrue((self.output / 'movies/0000.rvv').is_file())
        saved = json.loads((self.output / 'asset-manifest.json').read_text())
        self.assertEqual(saved, prepare.manifest(self.output))
        self.assertIn('movies/0000.rvv', [r['path'] for r in saved['assets']])
        self.assertEqual(saved['textures'], 2)
        self.assertEqual(self.staging_left(), [])

    def test_windows_local_tools_reach_movie_step(self):
        local = self.root / 'build-tools'
        local.mkdir()
        for name in ('ffmpeg.exe', 'ffprobe.exe'):
            (local / name).touch()
        with patch.object(host_tools.platform, 'system', return_value='Windows'), \
             patch.object(host_tools.shutil, 'which', return_value=None):
            prepare.export_iso([self.image], self.output, self.exporter, 'ffmpeg', 'ffprobe', 2)
        self.assertEqual(self.movies, [('ffmpeg.exe', 'ffprobe.exe')])
        self.assertTrue((self.output / 'asset-manifest.json').is_file())

    def test_windows_mixed_path_and_local_tools(self):
        local = self.root / 'build-tools'
        local.mkdir()
        (local / 'ffprobe.exe').touch()
        with patch.object(host_tools.platform, 'system', return_value='Windows'), \
             patch.object(host_tools.shutil, 'which', side_effect=lambda name: sys.executable if name == 'ffmpeg' else None):
            prepare.export_iso([self.image], self.output, self.exporter, 'ffmpeg', 'ffprobe', 2)
        self.assertEqual(self.movies, [(Path(sys.executable).name, 'ffprobe.exe')])

    def test_missing_windows_ffprobe_fails_before_extraction(self):
        local = self.root / 'build-tools'
        local.mkdir()
        (local / 'ffmpeg.exe').touch()
        with patch.object(host_tools.platform, 'system', return_value='Windows'), \
             patch.object(host_tools.shutil, 'which', return_value=None), \
             self.assertRaisesRegex(RuntimeError, 'build-tools/ffprobe.exe'):
            prepare.export_iso([self.image], self.output, self.exporter, 'ffmpeg', 'ffprobe', 2)
        self.assertFalse(self.output.exists())
        self.assertEqual(self.staging_left(), [])

    def test_failed_world_export_publishes_nothing(self):
        self.world.write_text(FAILING_WORLD)
        with self.assertRaisesRegex(RuntimeError, 'RM_0010: fixture decode failure'):
            self.run_pipeline()
        self.assertFalse(self.output.exists())
        self.assertEqual(self.movies, [])
        self.assertEqual(self.staging_left(), [])

    def test_existing_destination_is_untouched(self):
        self.output.mkdir()
        (self.output / 'user.txt').write_text('keep')
        with self.assertRaisesRegex(RuntimeError, 'destination exists'):
            self.run_pipeline()
        self.assertEqual([p.name for p in self.output.iterdir()], ['user.txt'])

    def test_missing_ffmpeg_fails_before_extraction(self):
        with self.assertRaisesRegex(RuntimeError, 'ffmpeg and ffprobe'):
            self.run_pipeline(ffmpeg=str(self.root / 'no-ffmpeg'))
        self.assertFalse(self.output.exists())
        self.assertEqual(self.staging_left(), [])

    def test_missing_world_exporter_fails_before_extraction(self):
        self.world.unlink()
        with self.assertRaisesRegex(RuntimeError, 'recvx-world-assets'):
            self.run_pipeline()
        self.assertFalse(self.output.exists())
        self.assertEqual(self.staging_left(), [])


if __name__ == '__main__':
    unittest.main()
