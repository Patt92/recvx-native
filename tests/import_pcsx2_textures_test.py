import hashlib, json, subprocess, sys, tempfile, unittest
from pathlib import Path
from PIL import Image
ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / 'tools'))
import import_pcsx2_textures as pcsx2


def cooked_tga(path, image):
    """Gekochte Form (prepare_native_data/write_tga): Typ 2, 32 Bit, oben links, ohne Fusszeile."""
    w, h = image.size
    header = bytes([0, 0, 2]) + bytes(9) + w.to_bytes(2, 'little') + h.to_bytes(2, 'little') + bytes([32, 0x28])
    path.parent.mkdir(parents=True, exist_ok=True)
    path.write_bytes(header + b''.join(bytes((b, g, r, a)) for r, g, b, a in image.convert('RGBA').getdata()))


def pattern(seed):
    image = Image.new('RGB', (32, 32))
    image.putdata([((x * seed * 7) % 256, (y * seed * 13) % 256, ((x + y) * seed * 3) % 256) for y in range(32) for x in range(32)])
    return image


class ImportPcsx2Tests(unittest.TestCase):
    def test_name_bits_give_the_ps2_size(self):
        # 0x1553: PSM 0x13 (PSMT8), TW 5, TH 5 -> 32x32; 0x2213 -> 256x256
        self.assertEqual(pcsx2.gs_size(0x1553), (32, 32))
        self.assertEqual(pcsx2.gs_size(0x2213), (256, 256))
        self.assertTrue(pcsx2.NAME.match('6e7f85af35d52ca-80c33d0945e691c7-00002213.dds'))
        self.assertTrue(pcsx2.NAME.match('c4c027817e67a47f-00001553.DDS'))
        self.assertFalse(pcsx2.NAME.match('tex1_128x128_0031c4697f8cfd83_14.dds'))

    def test_pack_is_matched_and_written_as_hd(self):
        with tempfile.TemporaryDirectory() as t:
            t = Path(t)
            for i, seed in enumerate((3, 5, 11)):
                cooked_tga(t / f'data/textures/rooms/RM_TEST/{i:04}.tga', pattern(seed))
            (t / 'pack').mkdir()
            pattern(5).resize((128, 128), Image.NEAREST).save(t / 'pack/1a2b3c-4d5e6f-00001553.dds')
            Image.new('RGB', (1024, 2048)).save(t / 'pack/77-88-00002653.dds')  # 512x512: keine gekochte Textur dieser Groesse
            result = subprocess.run([sys.executable, str(ROOT / 'tools/import_pcsx2_textures.py'), '--pack', str(t / 'pack'),
                                     '--data', str(t / 'data'), '--output', str(t / 'mods'), '--scale', '2', '--jobs', '2'],
                                    capture_output=True, text=True)
            self.assertEqual(result.returncode, 0, result.stderr)
            key = hashlib.md5((t / 'data/textures/rooms/RM_TEST/0001.tga').read_bytes()).hexdigest()
            hd = t / 'mods/textures/hd' / f'{key}.tga'
            self.assertTrue(hd.is_file(), result.stdout)
            self.assertEqual(Image.open(hd).size, (64, 64))
            manifest = json.loads((t / 'mods/textures/hd-manifest-pcsx2.json').read_text())
            self.assertEqual(manifest['matched'][key]['tier'], 'A')
            self.assertEqual(manifest['unmatched']['77-88-00002653.dds']['status'], 'no-candidate')
            self.assertEqual(len(list((t / 'mods/textures/hd').iterdir())), 1)


if __name__ == '__main__':
    unittest.main()
