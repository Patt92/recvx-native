"""Build-time ISO9660 extraction; synthetic images, no disc data needed."""
from pathlib import Path
import struct
import sys
import tempfile
import unittest

sys.path.insert(0, str(Path(__file__).resolve().parents[1] / 'tools'))
import iso9660


SECTOR = 2048


def both16(value):
    return struct.pack('<H', value) + struct.pack('>H', value)


def both32(value):
    return struct.pack('<I', value) + struct.pack('>I', value)


def record(name, lba, size, directory=False, more=False):
    identifier = name if isinstance(name, bytes) else name.encode('ascii')
    length = 33 + len(identifier)
    if length % 2:
        length += 1
    data = bytearray(length)
    data[0] = length
    data[2:10] = both32(lba)
    data[10:18] = both32(size)
    data[25] = (2 if directory else 0) | (0x80 if more else 0)
    data[28:32] = both16(1)
    data[32] = len(identifier)
    data[33:33 + len(identifier)] = identifier
    return bytes(data)


def directory(lba, parent, entries):
    data = bytearray()
    data += record(b'\x00', lba, SECTOR, directory=True)
    data += record(b'\x01', parent, SECTOR, directory=True)
    for entry in entries:
        data += entry
    return bytes(data).ljust(SECTOR, b'\x00')


def primary_volume(total_sectors, root_lba, root_size):
    data = bytearray(SECTOR)
    data[0] = 1
    data[1:6] = b'CD001'
    data[6] = 1
    data[40:72] = b'RECVX TEST'.ljust(32, b' ')
    data[80:88] = both32(total_sectors)
    data[120:124] = both16(1)
    data[124:128] = both16(1)
    data[128:132] = both16(SECTOR)
    data[140:144] = struct.pack('<I', 0)
    data[148:152] = struct.pack('>I', 0)
    data[156:190] = record(b'\x00', root_lba, root_size, directory=True)
    data[881] = 1
    return bytes(data)


def raw_sectors(image, layout):
    """Wrap 2048-byte blocks in a raw sector layout with recognisable filler."""
    size, offset = layout
    tail = size - offset - SECTOR
    out = bytearray()
    for i in range(0, len(image), SECTOR):
        head = b'\x00' + b'\xff' * 10 + b'\x00' + bytes([0, 2, i // SECTOR % 75, 1 if offset == 16 else 2])
        out += (head + bytes(8))[-offset:] if offset else b''
        out += image[i:i + SECTOR]
        out += bytes([0xEC]) * tail
    return bytes(out)


def build_flat(path, files, layout=(SECTOR, 0), boot_record=False):
    """Root directory with multi-sector files and a two-sector root directory."""
    root_lba = 20
    entries = []
    lba = 22
    blobs = {}
    for name, data in files:
        entries.append(record(name + ';1', lba, len(data)))
        blobs[lba] = data
        lba += max(1, (len(data) + SECTOR - 1) // SECTOR)
    # Fill the first root sector so the listing continues in the second one.
    first, second = bytearray(), bytearray()
    first += record(b'\x00', root_lba, 2 * SECTOR, directory=True)
    first += record(b'\x01', root_lba, 2 * SECTOR, directory=True)
    for entry in entries:
        (first if len(first) + len(entry) <= SECTOR and not second else second).extend(entry)
    sectors = [bytes(SECTOR)] * lba
    pvd_sector = 17 if boot_record else 16
    if boot_record:
        sectors[16] = (b'\x00CD001\x01').ljust(SECTOR, b'\x00')
    sectors[pvd_sector] = primary_volume(lba, root_lba, 2 * SECTOR)
    sectors[pvd_sector + 1] = (b'\xffCD001\x01').ljust(SECTOR, b'\x00')
    sectors[root_lba] = bytes(first).ljust(SECTOR, b'\x00')
    sectors[root_lba + 1] = bytes(second).ljust(SECTOR, b'\x00')
    for start, data in blobs.items():
        padded = data.ljust(max(1, (len(data) + SECTOR - 1) // SECTOR) * SECTOR, b'\x00')
        for k in range(0, len(padded), SECTOR):
            sectors[start + k // SECTOR] = padded[k:k + SECTOR]
    image = b''.join(sectors)
    if layout != (SECTOR, 0):
        image = raw_sectors(image, layout)
    Path(path).write_bytes(image)
    return Path(path)


def build_iso(path, system=b'HELLO', inner=b'INNER!', split=(b'abc', b'defg'),
              raw=False, unsafe_name=False):
    root_lba, sub_lba, system_lba, inner_lba, split_lba = 18, 19, 20, 21, 22
    root_entries = [
        record('SYSTEM.AFS;1', system_lba, len(system)),
        record('SUB;1', sub_lba, SECTOR, directory=True),
        record('SPLIT.BIN;1', split_lba, len(split[0]), more=True),
        record('SPLIT.BIN;1', split_lba + 1, len(split[1])),
    ]
    if unsafe_name:
        root_entries.append(record(b'../EVIL;1', system_lba, len(system)))
    sectors = [bytes(SECTOR)] * 24
    sectors[16] = primary_volume(len(sectors), root_lba, SECTOR)
    sectors[root_lba] = directory(root_lba, root_lba, root_entries)
    sectors[sub_lba] = directory(sub_lba, root_lba, [record('INNER.BIN;1', inner_lba, len(inner))])
    sectors[system_lba] = system.ljust(SECTOR, b'\x00')
    sectors[inner_lba] = inner.ljust(SECTOR, b'\x00')
    sectors[split_lba] = split[0].ljust(SECTOR, b'\x00')
    sectors[split_lba + 1] = split[1].ljust(SECTOR, b'\x00')
    image = b''.join(sectors)
    if raw:
        image = b''.join(bytes(16) + image[i:i + SECTOR] + bytes(288)
                         for i in range(0, len(image), SECTOR))
    Path(path).write_bytes(image)
    return Path(path)


class IsoTests(unittest.TestCase):
    def setUp(self):
        self.temporary = tempfile.TemporaryDirectory(prefix='recvx-iso-test-')
        self.addCleanup(self.temporary.cleanup)
        self.root = Path(self.temporary.name)

    def read(self, image, name):
        with iso9660.IsoImage(image) as opened:
            return b''.join(opened.chunks(opened.files[name]))

    def test_lists_files_and_strips_versions(self):
        image = build_iso(self.root / 'game.iso')
        with iso9660.IsoImage(image) as opened:
            self.assertEqual(opened.label, 'RECVX TEST')
            self.assertEqual(set(opened.files), {'SYSTEM.AFS', 'SUB/INNER.BIN', 'SPLIT.BIN'})
        self.assertEqual(self.read(image, 'SYSTEM.AFS'), b'HELLO')
        self.assertEqual(self.read(image, 'SUB/INNER.BIN'), b'INNER!')
        self.assertEqual(self.read(image, 'SPLIT.BIN'), b'abcdefg')

    def test_reads_raw_2352_image(self):
        image = build_iso(self.root / 'raw.iso', raw=True)
        self.assertEqual(self.read(image, 'SYSTEM.AFS'), b'HELLO')

    def test_raw_layouts_keep_multi_sector_files_and_directories(self):
        big = bytes((i * 7) % 251 for i in range(3 * SECTOR + 123))
        files = [('BIG.BIN', big)] + [(f'F{i:02}.BIN', bytes([i]) * (i + 1)) for i in range(60)]
        for layout in ((SECTOR, 0), (2352, 16), (2352, 24), (2336, 8)):
            with self.subTest(layout=layout):
                image = build_flat(self.root / f'flat-{layout[0]}-{layout[1]}.iso', files, layout)
                with iso9660.IsoImage(image) as opened:
                    self.assertEqual(len(opened.files), 61)
                    self.assertEqual(b''.join(opened.chunks(opened.files['BIG.BIN'])), big)
                    self.assertEqual(b''.join(opened.chunks(opened.files['F59.BIN'])), bytes([59]) * 60)

    def test_logical_blocks_are_absolute_behind_a_boot_record(self):
        image = build_flat(self.root / 'boot.iso', [('A.BIN', b'payload')], boot_record=True)
        self.assertEqual(self.read(image, 'A.BIN'), b'payload')

    def test_rejects_shared_directory_extents(self):
        # Two names for one subdirectory: an acyclic tree that grows exponentially.
        sectors = [bytes(SECTOR)] * 24
        sectors[16] = primary_volume(24, 18, SECTOR)
        sectors[18] = directory(18, 18, [record('A;1', 19, SECTOR, directory=True),
                                         record('B;1', 19, SECTOR, directory=True)])
        sectors[19] = directory(19, 18, [])
        image = self.root / 'dag.iso'
        image.write_bytes(b''.join(sectors))
        with self.assertRaises(iso9660.IsoError):
            iso9660.IsoImage(image)

    def test_rejects_case_only_and_windows_names(self):
        for names in (('DATA.BIN', 'data.bin'), ('CON.BIN',), ('A?.BIN',)):
            with self.subTest(names=names):
                image = build_flat(self.root / 'names.iso', [(n, b'x') for n in names])
                with self.assertRaises(iso9660.IsoError):
                    iso9660.IsoImage(image)

    def test_extracts_tree_and_merges_identical_images(self):
        image = build_iso(self.root / 'game.iso')
        target = self.root / 'extracted'
        files, total, count = iso9660.extract([image, image], target)
        self.assertEqual((files, total, count), (3, 18, 2))
        self.assertEqual((target / 'SYSTEM.AFS').read_bytes(), b'HELLO')
        self.assertEqual((target / 'SUB/INNER.BIN').read_bytes(), b'INNER!')
        self.assertEqual((target / 'SPLIT.BIN').read_bytes(), b'abcdefg')

    def test_conflicting_duplicate_is_rejected(self):
        first = build_iso(self.root / 'first.iso', system=b'HELLO')
        second = build_iso(self.root / 'second.iso', system=b'WORLD')
        with self.assertRaises(iso9660.IsoError):
            iso9660.extract([first, second], self.root / 'extracted')

    def test_rejects_non_iso_image(self):
        fake = self.root / 'fake.iso'
        fake.write_bytes(bytes(SECTOR * 40))
        with self.assertRaises(iso9660.IsoError):
            iso9660.IsoImage(fake)

    def test_rejects_unsafe_names(self):
        image = build_iso(self.root / 'evil.iso', unsafe_name=True)
        with self.assertRaises(iso9660.IsoError):
            iso9660.IsoImage(image)


if __name__ == '__main__':
    unittest.main()
