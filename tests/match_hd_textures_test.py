"""Regression coverage for the optional offline Dolphin texture matcher."""
import sys
import tempfile
import unittest
from pathlib import Path

import numpy as np
from PIL import Image

sys.path.insert(0, str(Path(__file__).resolve().parents[1] / 'tools'))
import match_hd_textures as matcher


class MatchHdTexturesTests(unittest.TestCase):
    def setUp(self):
        matcher._PIXELS.clear()
        matcher._SEARCH.clear()
        matcher._ALPHAS.clear()

    def test_flat_colours_are_not_identical(self):
        self.assertEqual(matcher.ncc(np.zeros(16), np.ones(16) * 200), 0)
        self.assertEqual(matcher.ncc(np.ones(16), np.ones(16)), 1)

    def candidates(self, root, images):
        values = []
        for i, image in enumerate(images):
            path = root / f'{i}.tga'
            image.save(path)
            pixels = np.asarray(image.convert('RGB'), dtype=np.float32)
            values.append((str(i), matcher.thumb(pixels), path.name))
        return {(32, 32): values}

    def test_fractional_resolution_and_equivalent_original_variants(self):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            rng = np.random.default_rng(42)
            pixels = rng.integers(20, 220, (32, 32, 3), dtype=np.uint8)
            base = Image.fromarray(pixels)
            variant = Image.fromarray(pixels + 2)
            other = Image.fromarray(rng.integers(0, 256, (32, 32, 3), dtype=np.uint8))
            candidates = self.candidates(root, [base, variant, other])
            source = root / 'tex1_32x32_example_14.dds'
            base.resize((120, 120), Image.Resampling.NEAREST).save(source)
            _, result = matcher.worker((str(source), candidates, root))
            self.assertEqual(result['status'], 'ok')
            self.assertEqual(result['scale'], 3.75)
            self.assertEqual(result['ps2'], '0')
            self.assertIn('1', result['equivalent_candidates'])
            self.assertTrue(result['colour_ok'])
            self.assertLess(result['ncc2'], .9)

    def test_different_alpha_masks_remain_competing_identities(self):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            rng = np.random.default_rng(3)
            base = Image.fromarray(rng.integers(0, 256, (32, 32, 3), dtype=np.uint8)).convert('RGBA')
            other = base.copy()
            other.putalpha(128)
            candidates = self.candidates(root, [base, other])
            source = root / 'tex1_32x32_example_14.dds'
            base.save(source)
            _, result = matcher.worker((str(source), candidates, root))
            self.assertEqual(result['equivalent_candidates'], [])
            self.assertEqual(result['ncc2'], 1)

    def test_changed_aspect_ratio_is_rejected(self):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            image = Image.new('RGB', (32, 32))
            candidates = self.candidates(root, [image])
            source = root / 'tex1_32x32_example_14.dds'
            image.resize((120, 100)).save(source)
            _, result = matcher.worker((str(source), candidates, root))
            self.assertEqual(result['status'], 'bad-scale')

    def test_spatial_search_recovers_match_outside_eight_colour_candidates(self):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            rng = np.random.default_rng(22)
            # All 4x4 blocks have the same mean: the old 8x8 thumbnail ties.
            images = []
            for i in range(41):
                pixels = np.zeros((32, 32, 3), dtype=np.uint8)
                for y in range(0, 32, 4):
                    for x in range(0, 32, 4):
                        values = rng.permutation(np.arange(16, dtype=np.uint8) * 16)
                        pixels[y:y+4, x:x+4] = values.reshape(4, 4, 1)
                images.append(Image.fromarray(pixels))
            candidates = self.candidates(root, images)
            source = root / 'tex1_32x32_example_14.dds'
            images[40].save(source)
            _, result = matcher.worker((str(source), candidates, root))
            self.assertEqual(result['ps2'], '40')
            self.assertEqual(result['ncc'], 1)

    def test_rgb_error_resolves_a_nearby_wrong_colour_variant(self):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            rng = np.random.default_rng(55)
            pixels = rng.integers(40, 170, (32, 32, 3), dtype=np.uint8)
            wrong_colour = Image.fromarray(pixels + 50)
            right_colour = Image.fromarray(pixels)
            candidates = self.candidates(root, [wrong_colour, right_colour])
            source = root / 'tex1_32x32_example_14.dds'
            right_colour.save(source)
            _, result = matcher.worker((str(source), candidates, root))
            self.assertEqual(result['ps2'], '1')
            self.assertTrue(result['decisive_rgb'])
            self.assertEqual(result['rgb_error_ratio'], 0)


if __name__ == '__main__':
    unittest.main()
