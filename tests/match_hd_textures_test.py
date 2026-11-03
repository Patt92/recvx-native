"""Regression coverage for the optional offline Dolphin texture matcher."""
import json
import sys
import tempfile
import unittest
from pathlib import Path

import numpy as np
from PIL import Image, ImageFilter

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

    def test_cross_size_uses_ps2_resolution_and_preserves_gc_metadata(self):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            rng = np.random.default_rng(31)
            image = Image.fromarray(rng.integers(20, 200, (32, 32, 3), dtype=np.uint8))
            candidates = self.candidates(root, [image])
            source = root / 'tex1_64x64_m_example_14.dds'
            image.resize((256, 256), Image.Resampling.NEAREST).save(source)
            _, same_size = matcher.worker((str(source), candidates, root))
            self.assertEqual(same_size['status'], 'no-size')
            _, cross = matcher.worker((str(source), candidates, root, True))
            self.assertEqual(cross['source_size'], [64, 64])
            self.assertEqual(cross['target_size'], [32, 32])
            self.assertEqual(cross['scale'], 4)
            self.assertEqual(cross['ncc'], 1)
            self.assertTrue(cross['cross_size_ok'])

    def test_cross_size_gap_rule_replaces_psnr_floor_only_with_clear_lead(self):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            rng = np.random.default_rng(81)
            pixels = rng.integers(40, 200, (32, 32, 3), dtype=np.uint8)
            original = Image.fromarray(pixels)
            other = Image.fromarray(rng.integers(0, 256, (32, 32, 3), dtype=np.uint8))
            candidates = self.candidates(root, [original, other])
            source = root / 'tex1_64x64_soft_14.dds'
            # Lost sharpening (a larger GameCube original reduced to the PS2 size) is low-frequency.
            original.filter(ImageFilter.GaussianBlur(0.4)).resize((256, 256), Image.Resampling.NEAREST).save(source)
            _, result = matcher.worker((str(source), candidates, root, True))
            self.assertEqual(result['ps2'], '0')
            self.assertLess(result['psnr'], 26)
            self.assertGreaterEqual(result['psnr'], 23)
            self.assertTrue(result['cross_size_ok'])
            self.assertEqual(result['cross_size_rule'], 'gap')
            near = Image.fromarray(np.clip(pixels.astype(np.int16) + rng.integers(-20, 21, pixels.shape), 0, 255).astype(np.uint8))
            candidates = self.candidates(root, [original, near])
            self.setUp()   # candidate '1' changed on disk: drop cached pixels
            _, result = matcher.worker((str(source), candidates, root, True))
            self.assertLess(result['ncc'] - result['ncc2'], matcher.CROSS_SIZE_GAP)
            self.assertFalse(result['cross_size_ok'])
            self.assertIsNone(result['cross_size_rule'])

    def test_cross_size_does_not_accept_random_same_aspect_image(self):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            rng = np.random.default_rng(76)
            originals = [Image.fromarray(rng.integers(0, 256, (32, 32, 3), dtype=np.uint8))]
            candidates = self.candidates(root, originals)
            source = root / 'tex1_64x64_other_14.dds'
            Image.fromarray(rng.integers(0, 256, (256, 256, 3), dtype=np.uint8)).save(source)
            _, result = matcher.worker((str(source), candidates, root, True))
            self.assertFalse(result['cross_size_ok'])

    def test_cross_size_rejects_incompatible_visible_alpha_layout(self):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            rng = np.random.default_rng(78)
            original = Image.fromarray(rng.integers(20, 200, (32, 32, 3), dtype=np.uint8)).convert('RGBA')
            mask = np.zeros((32, 32), dtype=np.uint8);mask[:, :16] = 255
            original.putalpha(Image.fromarray(mask))
            candidates = self.candidates(root, [original])
            source = root / 'tex1_64x64_alpha_14.dds'
            pack = original.copy();pack.putalpha(Image.fromarray(255-mask))
            pack.resize((256, 256), Image.Resampling.NEAREST).save(source)
            _, result = matcher.worker((str(source), candidates, root, True))
            self.assertEqual(result['ncc'], 1)
            self.assertFalse(result['cross_size_ok'])
            self.assertLess(result['alpha_layout_ncc'], 0)

    def test_alpha_mask_ignores_noise_under_transparent_texels(self):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            rng = np.random.default_rng(91)
            pixels = rng.integers(20, 220, (32, 32, 3), dtype=np.uint8)
            pixels[:, 16:] = 0            # original fill under transparent texels
            mask = np.zeros((32, 32), dtype=np.uint8);mask[:, :16] = 255
            original = Image.fromarray(pixels).convert('RGBA');original.putalpha(Image.fromarray(mask))
            other = Image.fromarray(rng.integers(0, 256, (32, 32, 3), dtype=np.uint8))
            candidates = self.candidates(root, [original, other])
            noisy = pixels.copy();noisy[:, 16:] = rng.integers(0, 256, (32, 16, 3), dtype=np.uint8)
            pack = Image.fromarray(noisy).convert('RGBA');pack.putalpha(Image.fromarray(mask))
            source = root / 'tex1_32x32_noise_14.dds'
            pack.resize((128, 128), Image.Resampling.NEAREST).save(source)
            _, plain = matcher.worker((str(source), candidates, root, False, False))
            _, masked = matcher.worker((str(source), candidates, root, False, True))
            self.assertLess(plain['ncc'], .9)
            self.assertEqual(masked['ps2'], '0')
            self.assertEqual(masked['ncc'], 1)
            self.assertTrue(masked['masked'])
            self.assertEqual(masked['coverage'], .5)
            self.assertTrue(masked['alpha_ok'])
            self.assertTrue(masked['colour_ok'])
            self.assertNotIn('alpha_ok', plain)

    def test_alpha_mask_requires_alpha_channel_for_nearly_invisible_sources(self):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            rng = np.random.default_rng(92)
            pixels = rng.integers(20, 220, (32, 32, 3), dtype=np.uint8)
            alpha = np.zeros((32, 32), dtype=np.uint8);alpha[2:6, 2:6] = 40   # 1.6 % faint, nothing >= 128
            original = Image.fromarray(pixels).convert('RGBA');original.putalpha(Image.fromarray(alpha))
            candidates = self.candidates(root, [original])
            same = root / 'tex1_32x32_same_14.dds'
            original.save(same)
            _, result = matcher.worker((str(same), candidates, root, False, True))
            self.assertFalse(result['masked'])
            self.assertTrue(result['alpha_ok'])
            self.assertGreaterEqual(result['alpha_full_ncc'], .99)
            moved = original.copy();moved.putalpha(Image.fromarray(np.roll(alpha, 16, axis=1)))
            elsewhere = root / 'tex1_32x32_moved_14.dds'
            moved.save(elsewhere)
            _, result = matcher.worker((str(elsewhere), candidates, root, False, True))
            self.assertFalse(result['alpha_ok'])

    def test_claimed_original_no_longer_competes(self):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            rng = np.random.default_rng(93)
            pixels = rng.integers(20, 220, (32, 32, 3), dtype=np.uint8)
            lit = Image.fromarray(pixels)
            dark = Image.fromarray((pixels * 0.9).astype(np.uint8))   # near-identical lighting variant
            far = Image.fromarray(rng.integers(0, 256, (32, 32, 3), dtype=np.uint8))
            candidates = self.candidates(root, [lit, dark, far])
            source = root / 'tex1_32x32_dark_14.dds'
            upscaled = np.asarray(dark.resize((128, 128), Image.Resampling.NEAREST), dtype=np.int16)
            Image.fromarray(np.clip(upscaled + rng.integers(-40, 41, upscaled.shape), 0, 255).astype(np.uint8)).save(source)
            _, free = matcher.worker((str(source), candidates, root))
            self.assertEqual(free['ps2'], '1')
            self.assertLess(free['ncc'], 1)
            self.assertGreater(free['ncc2'], .95)          # variants compete: ambiguous for the builder
            claimed = {'1': {'dds': 'tex1_32x32_other_14.dds', 'ncc': 1.0}}
            _, displaced = matcher.worker((str(source), candidates, root, False, False, claimed))
            self.assertEqual(displaced['ps2'], '0')
            self.assertEqual(displaced['eliminated'], [['1', free['ncc'], 'tex1_32x32_other_14.dds']])
            self.assertLess(displaced['ncc2'], .5)
            _, displaced = matcher.worker((str(source), candidates, root, False, True, claimed))
            self.assertEqual(displaced['ps2'], '0')   # alpha mode records the elimination too
            self.assertEqual(displaced['eliminated'][0][::2], ['1', 'tex1_32x32_other_14.dds'])
            own = {'1': {'dds': 'tex1_32x32_dark_14.dds', 'ncc': 1.0}}
            _, kept = matcher.worker((str(source), candidates, root, False, False, own))
            self.assertEqual(kept['ps2'], '1')
            self.assertNotIn('eliminated', kept)
            weaker = {'1': {'dds': 'tex1_32x32_other_14.dds', 'ncc': free['ncc'] - .01}}
            _, kept = matcher.worker((str(source), candidates, root, False, False, weaker))
            self.assertEqual(kept['ps2'], '1')
            palette_variant = {'1': {'dds': 'tex1_32x32_dark_0123456789abcdef_9.dds', 'ncc': 1.0}}
            _, kept = matcher.worker((str(source), candidates, root, False, False, palette_variant))
            self.assertEqual(kept['ps2'], '1')   # same GameCube texture hash: not a competitor
            self.assertNotIn('eliminated', kept)

    def test_load_claims_merges_manifest_and_match_table(self):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            manifest = root / 'hd-manifest.json'
            manifest.write_text(json.dumps({'a': {'dds': 'tex1_32x32_1_14.dds', 'ncc': .95, 'tier': 'A', 'ps2': 'rooms/x.tga'}}))
            table = root / 'matches.json'
            table.write_text(json.dumps({'ps2': {'a': ['rooms/x.tga'], 'b': ['rooms/y.tga']}, 'matches': {
                'pack/tex1_32x32_2_14.dds': {'status': 'ok', 'ps2': 'a', 'ncc': .97, 'ncc2': .5},
                'pack/tex1_32x32_3_14.dds': {'status': 'ok', 'ps2': 'b', 'ncc': .80, 'ncc2': .3},
                'pack/tex1_32x32_4_14.dds': {'status': 'ok', 'ps2': 'b', 'ncc': .90, 'ncc2': .3},
                'pack/tex1_32x32_6_14.dds': {'status': 'ok', 'ps2': 'b', 'ncc': .99, 'ncc2': .98},   # ambiguous: no claim
                'pack/tex1_32x32_7_14.dds': {'status': 'ok', 'ps2': 'b', 'ncc': .99, 'ncc2': .5, 'colour_ok': False},
                'pack/tex1_32x32_5_14.dds': {'status': 'no-size'}}}))
            claims = matcher.load_claims([manifest, table])
            self.assertEqual(claims, {'a': {'dds': 'tex1_32x32_2_14.dds', 'ncc': .97}, 'b': {'dds': 'tex1_32x32_4_14.dds', 'ncc': .90}})
            self.assertEqual(matcher.texture_hash('tex1_512x512_05ffbe0a83ca28cd_c70b769b5c4666a7_9.dds'), '05ffbe0a83ca28cd')
            self.assertEqual(matcher.texture_hash('tex1_64x64_m_example_14.dds'), 'example')

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

    def test_indistinguishable_runner_up_does_not_compete(self):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            rng = np.random.default_rng(11)
            pixels = rng.integers(40, 200, (32, 32, 3), dtype=np.int16)
            base = Image.fromarray(pixels.astype(np.uint8))
            # A room variant of the same picture: closer to the base than the pack image is.
            variant = Image.fromarray((pixels + rng.integers(-25, 26, pixels.shape)).clip(0, 255).astype(np.uint8))
            other = Image.fromarray(rng.integers(0, 256, (32, 32, 3), dtype=np.uint8))
            candidates = self.candidates(root, [base, variant, other])
            source = root / 'tex1_32x32_example_14.dds'
            pack = (pixels + rng.integers(-40, 41, pixels.shape)).clip(0, 255).astype(np.uint8)
            Image.fromarray(pack).resize((128, 128), Image.Resampling.NEAREST).save(source)
            _, result = matcher.worker((str(source), candidates, root))
            self.assertEqual(result['ps2'], '0')
            self.assertEqual(result['equivalent_candidates'], [])
            self.assertEqual([item[0] for item in result['indistinguishable_candidates']], ['1'])
            self.assertGreaterEqual(result['indistinguishable_candidates'][0][1], result['ncc'])
            self.assertLess(result['ncc2'], .5)
            # The same variant competes again once the pack image fits the base better than the variant does.
            self.setUp()
            base.resize((128, 128), Image.Resampling.NEAREST).save(source)
            _, result = matcher.worker((str(source), candidates, root))
            self.assertEqual(result['indistinguishable_candidates'], [])
            self.assertGreater(result['ncc2'], .9)

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
