import subprocess, sys, tempfile, unittest
from pathlib import Path
from PIL import Image
ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / 'tools'))
import prepare_native_data as pnd

def decode_colour_mapped(data):
    """TGA Typ 9 (8-Bit-Index, RLE) zurueck zur unkomprimierten Typ-2-Datei (Pillow liest 32-Bit-Tabellen nicht)."""
    first, length, entry = int.from_bytes(data[3:5], 'little'), int.from_bytes(data[5:7], 'little'), data[7] // 8
    w, h = int.from_bytes(data[12:14], 'little'), int.from_bytes(data[14:16], 'little')
    start = 18 + data[0]
    table, at, index = data[start:start + length * entry], start + length * entry, bytearray()
    while len(index) < w * h:
        head = data[at]; n = (head & 0x7f) + 1; at += 1
        if head & 0x80:
            index += bytes([data[at]]) * n; at += 1
        else:
            index += data[at:at + n]; at += n
    header = bytearray(data[:start]); header[1] = 0; header[2] = 2; header[3:8] = bytes(5); header[16] = entry * 8
    return bytes(header) + b''.join(table[(k - first) * entry:(k - first + 1) * entry] for k in index)


class CompressedTreeTests(unittest.TestCase):
    def test_texture_compressor_sits_next_to_the_exporter(self):
        self.assertEqual(pnd.texture_compressor_for(Path('/b/recvx-assets')), Path('/b/recvx-texture-compress'))
        self.assertEqual(pnd.texture_compressor_for(Path('/b/recvx-assets.exe')), Path('/b/recvx-texture-compress.exe'))

    def test_colour_mapped_tree_is_pixel_identical(self):
        tool = next((p for p in (ROOT / 'build').glob('*/recvx-texture-compress') if p.is_file()), None)
        if tool is None:
            self.skipTest('recvx-texture-compress not built')
        with tempfile.TemporaryDirectory() as t:
            src, dst = Path(t) / 'src', Path(t) / 'dst'
            img = Image.new('RGBA', (64, 32), (10, 20, 30, 128)); img.putpixel((3, 5), (255, 0, 0, 7))
            (src / 'rooms/RM_TEST').mkdir(parents=True)
            raw = bytes([0, 0, 2]) + bytes(9) + (64).to_bytes(2, 'little') + (32).to_bytes(2, 'little') + bytes([32, 0x28])
            raw += b''.join(bytes((b, g, r, a)) for r, g, b, a in img.getdata())  # gekochte Form: ohne Fusszeile
            (src / 'rooms/RM_TEST/0000.tga').write_bytes(raw)
            subprocess.run([str(tool), str(src), str(dst), '2'], check=True, capture_output=True)
            out = dst / 'rooms/RM_TEST/0000.tga'
            self.assertEqual(out.read_bytes()[1:3], bytes([1, 9]))
            self.assertEqual(decode_colour_mapped(out.read_bytes()), (src / 'rooms/RM_TEST/0000.tga').read_bytes())
            self.assertNotEqual(subprocess.run([str(tool), str(src), str(dst)], capture_output=True).returncode, 0)

    def test_compressed_needs_exactly_one_source(self):
        r = subprocess.run([sys.executable, str(ROOT / 'tools/prepare_native_data.py'), '--output', 'x', '--compressed'],
                           capture_output=True, text=True)
        self.assertNotEqual(r.returncode, 0); self.assertIn('--iso or --from-data', r.stderr)

if __name__ == '__main__':
    unittest.main()
