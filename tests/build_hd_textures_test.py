"""Selection, ownership and output checks for the offline HD-pack builder."""
import json
import subprocess
import sys
import tempfile
import unittest
from pathlib import Path

from PIL import Image

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / 'tools'))
import build_hd_textures as builder


def match(key='target', ncc=.99, **extra):
    return dict(status='ok', ps2=key, ncc=ncc, ncc2=.1, psnr=30, scale=4, **extra)


class BuildHdTexturesTests(unittest.TestCase):
    def test_preserves_existing_and_reports_every_source(self):
        matches = {'a': match('old'), 'b': match(), 'c': match(ncc=.98),
                   'd': {'status': 'no-size'}, 'e': match('colour', colour_ok=False)}
        chosen, report = builder.select_matches(matches, {'old': {'dds': 'original.dds'}})
        self.assertEqual(set(chosen), {'target'})
        self.assertEqual(chosen['target'][0], 'b')
        self.assertEqual(set(report), set(matches))
        self.assertEqual(report['a']['status'], 'existing-target-kept')
        self.assertEqual(report['c']['status'], 'alternate-source')
        self.assertEqual(report['c']['selected'], 'b')
        self.assertEqual(report['e']['status'], 'colour-mismatch')

    def test_alternate_source_points_to_final_winner(self):
        chosen, report = builder.select_matches({'a': match(ncc=.96), 'b': match(ncc=.97), 'c': match()})
        self.assertEqual(chosen['target'][0], 'c')
        self.assertEqual(report['a']['selected'], 'c')
        self.assertEqual(report['b']['selected'], 'c')

    def test_existing_source_cannot_be_reassigned_to_a_different_target(self):
        chosen, report = builder.select_matches({'pack/same.dds': match('new')}, {'old': {'dds': 'same.dds'}})
        self.assertEqual(chosen, {})
        self.assertEqual(report['pack/same.dds'], {'status': 'existing-source-kept', 'target': 'old'})

    def test_exact_error_evidence_does_not_bypass_colour_rejection(self):
        self.assertEqual(builder.tier(match(decisive_rgb=True, colour_ok=False)), None)
        self.assertEqual(builder.tier(match(decisive_rgb=True, colour_ok=True)), 'A')

    def test_cli_limit_manifest_only_lists_emitted_files(self):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            textures = root / 'textures'
            textures.mkdir()
            image = Image.new('RGBA', (16, 16), (25, 50, 100, 128))
            image.save(textures / 'base.tga')
            source = root / 'source.dds'
            image.resize((64, 64)).save(source)
            table = {'ps2': {'one': ['base.tga'], 'two': ['base.tga']},
                     'matches': {str(source): match('one'), str(root / 'second.dds'): match('two')}}
            source.replace(root / 'second.dds')
            image.save(source)
            (root / 'matches.json').write_text(json.dumps(table))
            existing = {'old': {'dds': 'keep.dds'}}
            (root / 'existing.json').write_text(json.dumps(existing))
            subprocess.run([sys.executable, str(ROOT / 'tools/build_hd_textures.py'),
                            '--matches', str(root / 'matches.json'), '--textures', str(textures),
                            '--output', str(root / 'mods'), '--limit', '1', '--scale', '2',
                            '--existing-manifest', str(root / 'existing.json'),
                            '--report', str(root / 'report.json')], check=True, capture_output=True)
            manifest = json.loads((root / 'mods/textures/hd-manifest.json').read_text())
            self.assertEqual(manifest['old'], existing['old'])
            self.assertEqual(len(manifest), 2)
            emitted = [key for key in manifest if key != 'old']
            self.assertTrue((root / 'mods/textures/hd' / (emitted[0] + '.tga')).is_file())
            with Image.open(root / 'mods/textures/hd' / (emitted[0] + '.tga')) as result:
                self.assertEqual(result.size, (32, 32))
                self.assertEqual(result.getchannel('A').getextrema(), (128, 128))
            self.assertEqual(sum(v['status'] == 'filtered-out' for v in json.loads((root / 'report.json').read_text()).values()), 1)

    def test_explicit_overrides_cover_every_identity_of_uncooked_target(self):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            textures = root / 'textures'
            paths = ['weapons/0021/right/0000.tga', 'weapons/0051/right/0000.tga']
            for path in paths:
                (textures / path).parent.mkdir(parents=True)
                Image.new('RGB', (16, 16), 'red').save(textures / path)
            source = root / 'source.dds'
            Image.new('RGB', (64, 64), 'red').save(source)
            (root / 'matches.json').write_text(json.dumps({'ps2': {'key': paths}, 'matches': {str(source): match('key')}}))
            subprocess.run([sys.executable, str(ROOT / 'tools/build_hd_textures.py'), '--matches', str(root / 'matches.json'),
                            '--textures', str(textures), '--output', str(root / 'mods'), '--per-asset', 'weapons'],
                           check=True, capture_output=True)
            for path in paths:
                self.assertTrue((root / 'mods/textures' / path).is_file())
            manifest = json.loads((root / 'mods/textures/hd-manifest.json').read_text())
            self.assertEqual(set(manifest['key']['overrides']), {'textures/' + path for path in paths})

    def test_existing_hash_file_is_projected_to_uncooked_identity_without_conversion(self):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            source = root / 'existing/hd/key.tga'
            source.parent.mkdir(parents=True)
            Image.new('RGB', (16, 16), (50, 60, 70)).save(source)
            (root / 'existing/hd-manifest.json').write_text(json.dumps({'key': {'dds': 'old.dds'}}))
            paths = ['weapons/0021/right/0000.tga']
            (root / 'matches.json').write_text(json.dumps({'ps2': {'key': paths}, 'matches': {}}))
            subprocess.run([sys.executable, str(ROOT / 'tools/build_hd_textures.py'), '--matches', str(root / 'matches.json'),
                            '--output', str(root / 'mods'), '--existing-manifest', str(root / 'existing/hd-manifest.json'),
                            '--per-asset', 'weapons'], check=True, capture_output=True)
            destination = root / 'mods/textures' / paths[0]
            self.assertEqual(destination.read_bytes(), source.read_bytes())


if __name__ == '__main__':
    unittest.main()
