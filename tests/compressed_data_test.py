import subprocess, sys, tempfile, unittest
from pathlib import Path
from PIL import Image
ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / 'tools'))
import prepare_native_data as pnd

class CompressedTreeTests(unittest.TestCase):
    def test_rle_tga_is_pixel_identical(self):
        with tempfile.TemporaryDirectory() as t:
            src, dst = Path(t) / 'a.tga', Path(t) / 'o/b.tga'
            img = Image.new('RGBA', (64, 32), (10, 20, 30, 128)); img.putpixel((3, 5), (255, 0, 0, 7)); img.save(src)
            pnd.rle_tga((src, dst))
            self.assertEqual(dst.read_bytes()[2], 10)
            self.assertEqual(Image.open(dst).tobytes(), Image.open(src).tobytes())

    def test_compressed_needs_exactly_one_source(self):
        r = subprocess.run([sys.executable, str(ROOT / 'tools/prepare_native_data.py'), '--output', 'x', '--compressed'],
                           capture_output=True, text=True)
        self.assertNotEqual(r.returncode, 0); self.assertIn('--iso or --from-data', r.stderr)

if __name__ == '__main__':
    unittest.main()
