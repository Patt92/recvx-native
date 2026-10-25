import hashlib, json, sys, tempfile, unittest
from pathlib import Path
from PIL import Image
ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / 'tools'))
import import_hd_textures as hd_import


def cooked_tga(path, image):
    """Gekochte Form (prepare_native_data/write_tga): Typ 2, 32 Bit, oben links, ohne Fusszeile."""
    w, h = image.size
    header = bytes([0, 0, 2]) + bytes(9) + w.to_bytes(2, 'little') + h.to_bytes(2, 'little') + bytes([32, 0x28])
    path.parent.mkdir(parents=True, exist_ok=True)
    path.write_bytes(header + image.convert('RGBA').tobytes('raw', 'BGRA'))


def pattern(seed):
    image = Image.new('RGB', (32, 32))
    image.putdata([((x * seed * 7) % 256, (y * seed * 13) % 256, ((x + y) * seed * 3) % 256) for y in range(32) for x in range(32)])
    return image


class ImportHdTexturesTests(unittest.TestCase):
    def test_names_give_the_original_size(self):
        # PCSX2 0x1553: TW 5, TH 5 -> 32x32; 0x2213 -> 256x256; Dolphin: Groesse im Namen; Mip-Stufen nicht
        self.assertEqual(hd_import.name_size('1a2b3c-4d5e6f-00001553.dds'), (32, 32))
        self.assertEqual(hd_import.name_size('c4c027817e67a47f-00002213.DDS'), (256, 256))
        self.assertEqual(hd_import.name_size('tex1_128x64_0031c4697f8cfd83_14.dds'), (128, 64))
        self.assertIsNone(hd_import.name_size('tex1_128x64_0031c4697f8cfd83_14_mip1.dds'))
        self.assertIsNone(hd_import.name_size('readme.dds'))

    def test_hd_source_is_matched_written_and_withdrawn(self):
        with tempfile.TemporaryDirectory() as t:
            t = Path(t)
            data, mods = t / 'data', t / 'mods'
            for i, seed in enumerate((3, 5, 11)):
                cooked_tga(data / f'textures/rooms/RM_TEST/{i:04}.tga', pattern(seed))
            cooked_tga(data / 'textures/rooms/RM_COPY/0000.tga', pattern(5))  # gleicher Inhalt, zweiter Raum
            source = mods / 'hd-source/pcsx2'
            source.mkdir(parents=True)
            pattern(5).resize((128, 128), Image.NEAREST).save(source / '1a2b3c-4d5e6f-00001553.dds')
            Image.new('RGB', (1024, 2048)).save(source / '77-88-00002653.dds')  # 512x512: keine gekochte Textur dieser Groesse
            Image.new('RGB', (8, 8)).save(source / 'notes.dds')
            foreign = mods / 'textures/hd/feedface.tga'
            foreign.parent.mkdir(parents=True)
            foreign.write_bytes(b'from build_hd_textures')
            log = []
            record = hd_import.run(mods, data, 4, 'AB', 2, log=log.append)

            digest = hashlib.md5((data / 'textures/rooms/RM_TEST/0001.tga').read_bytes()).hexdigest()
            written = mods / 'textures/hd' / f'{digest}.tga'
            self.assertEqual(record['written'], [written.name])
            self.assertEqual(record['matched'][digest]['files'], 2)
            self.assertEqual(record['matched'][digest]['dds'], 'pcsx2/1a2b3c-4d5e6f-00001553.dds')
            self.assertIn('pcsx2/77-88-00002653.dds', record['unused'])
            self.assertEqual(record['ignored_names'], ['pcsx2/notes.dds'])
            head = written.read_bytes()[:18]
            self.assertEqual((head[2], int.from_bytes(head[12:14], 'little'), head[16], head[17]), (2, 128, 32, 0x28))
            self.assertEqual(json.loads((mods / 'textures/hd-import.json').read_text())['written'], [written.name])
            self.assertIn('1 matched (2 cooked files)', log[-1])

            log.clear()
            hd_import.run(mods, data, 4, 'AB', 2, log=log.append)
            self.assertTrue(log[-1].startswith('HD import up to date'), log)

            for dds in source.iterdir():
                dds.unlink()
            hd_import.run(mods, data, 4, 'AB', 2, log=log.append)
            self.assertFalse(written.exists())
            self.assertEqual(foreign.read_bytes(), b'from build_hd_textures')  # fremde Dateien bleiben
            self.assertFalse((mods / 'textures/hd-import.json').exists())

    def test_redrawn_atlas_matches_by_layout_and_keeps_its_alpha(self):
        # Neu gezeichneter Atlas: Bild weicht ab (Rauschen, andere Farbe unter Transparenz), Maske gleich.
        checker = Image.new('L', (32, 32))
        checker.putdata([255 if (x // 8 + y // 8) % 2 == 0 else 0 for y in range(32) for x in range(32)])
        with tempfile.TemporaryDirectory() as t:
            t = Path(t)
            data, mods = t / 'data', t / 'mods'
            atlas = Image.composite(pattern(5), Image.new('RGB', (32, 32)), checker).convert('RGBA')
            atlas.putalpha(checker)
            cooked_tga(data / 'textures/menu/0001.tga', atlas)
            other = pattern(11).convert('RGBA')
            other.putalpha(Image.eval(checker, lambda a: 255 - a))
            cooked_tga(data / 'textures/menu/0002.tga', other)
            noise = Image.effect_noise((32, 32), 120).convert('RGB')
            redrawn = Image.composite(Image.blend(pattern(5), noise, 0.35), Image.new('RGB', (32, 32), (255, 255, 255)), checker)
            redrawn = redrawn.resize((128, 128), Image.NEAREST).convert('RGBA')
            redrawn.putalpha(Image.eval(checker, lambda a: 0x80 if a else 0).resize((128, 128), Image.NEAREST))  # GS-Alpha 0x80
            (mods / 'hd-source').mkdir(parents=True)
            redrawn.save(mods / 'hd-source/aa-bb-00001553.dds')
            record = hd_import.run(mods, data, 4, 'ABL', 1, log=lambda _: None)
            digest = hashlib.md5((data / 'textures/menu/0001.tga').read_bytes()).hexdigest()
            self.assertEqual(record['matched'][digest]['tier'], 'L', record)
            self.assertEqual(record['matched'][digest]['alpha'], 'pack')
            written = Image.open(mods / 'textures/hd' / f'{digest}.tga')
            self.assertEqual(written.getchannel('A').getextrema(), (0, 255))  # GS-Bereich verdoppelt
            hd_import.run(mods, data, 4, 'AB', 1, force=True, log=lambda _: None)
            self.assertFalse((mods / 'textures/hd' / f'{digest}.tga').exists(), 'ohne Stufe L kein Treffer')


if __name__ == '__main__':
    unittest.main()
