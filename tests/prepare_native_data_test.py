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
