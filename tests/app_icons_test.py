"""Check native icon containers and optionally their embedded Windows PE images."""
from pathlib import Path
import struct
import sys
import unittest

ROOT = Path(__file__).resolve().parents[1]


def ico_images(data):
    reserved, kind, count = struct.unpack_from('<HHH', data)
    assert (reserved, kind) == (0, 1)
    images = {}
    for index in range(count):
        width, height, _, _, _, _, size, offset = struct.unpack_from('<BBBBHHII', data, 6 + index * 16)
        images[(width or 256, height or 256)] = data[offset:offset + size]
        assert len(images[(width or 256, height or 256)]) == size
    return images


def pe_icons(path):
    data = path.read_bytes()
    pe = struct.unpack_from('<I', data, 0x3c)[0]
    assert data[pe:pe + 4] == b'PE\0\0'
    machine, count = struct.unpack_from('<HH', data, pe + 4)
    assert machine == 0x8664, 'expected Windows x64'
    optional_size = struct.unpack_from('<H', data, pe + 20)[0]
    optional = pe + 24
    assert struct.unpack_from('<H', data, optional)[0] == 0x20b
    resource_rva = struct.unpack_from('<I', data, optional + 112 + 2 * 8)[0]
    sections = []
    for index in range(count):
        header = optional + optional_size + index * 40
        virtual_size, address, raw_size, offset = struct.unpack_from('<IIII', data, header + 8)
        sections.append((address, max(virtual_size, raw_size), offset))

    def file_offset(rva):
        for address, size, offset in sections:
            if address <= rva < address + size:
                return offset + rva - address
        raise AssertionError(f'RVA outside PE sections: {rva:x}')

    base = file_offset(resource_rva)
    resources = {}

    def directory(relative, keys):
        named, numbered = struct.unpack_from('<HH', data, base + relative + 12)
        for index in range(named + numbered):
            key, child = struct.unpack_from('<II', data, base + relative + 16 + index * 8)
            if child & 0x80000000:
                directory(child & 0x7fffffff, keys + (key,))
            else:
                rva, size = struct.unpack_from('<II', data, base + child)
                offset = file_offset(rva)
                resources[keys + (key,)] = data[offset:offset + size]

    directory(0, ())
    return resources


class AppIconTests(unittest.TestCase):
    def test_ico_sizes_and_png_payloads(self):
        images = ico_images((ROOT / 'native/resources/app-icon.ico').read_bytes())
        self.assertEqual(set(images), {(size, size) for size in (16, 24, 32, 48, 64, 128, 256)})
        for dimensions, image in images.items():
            self.assertEqual(image[:8], b'\x89PNG\r\n\x1a\n')
            self.assertEqual(struct.unpack_from('>II', image, 16), dimensions)

    def test_icns_container_and_retina(self):
        data = (ROOT / 'native/resources/app-icon.icns').read_bytes()
        self.assertEqual(data[:4], b'icns')
        self.assertEqual(struct.unpack_from('>I', data, 4)[0], len(data))
        position, dimensions = 8, set()
        while position < len(data):
            kind, size = struct.unpack_from('>4sI', data, position)
            self.assertGreaterEqual(size, 8)
            self.assertLessEqual(position + size, len(data))
            image = data[position + 8:position + size]
            if image[:8] == b'\x89PNG\r\n\x1a\n':
                dimensions.add(struct.unpack_from('>II', image, 16))
            position += size
        self.assertEqual(position, len(data))
        self.assertTrue({(32, 32), (64, 64), (128, 128), (256, 256), (512, 512), (1024, 1024)} <= dimensions)


def check_exe(path):
    resources = pe_icons(path)
    expected = ico_images((ROOT / 'native/resources/app-icon.ico').read_bytes())
    groups = {key: value for key, value in resources.items() if key[0] == 14}
    assert len(groups) == 1, 'expected one RT_GROUP_ICON'
    key, group = next(iter(groups.items()))
    assert key[1] == 1, 'expected first application icon, ID 1'
    reserved, kind, count = struct.unpack_from('<HHH', group)
    assert (reserved, kind, count) == (0, 1, len(expected))
    for index in range(count):
        width, height, _, _, _, _, size, icon_id = struct.unpack_from('<BBBBHHIH', group, 6 + index * 14)
        image = resources[(3, icon_id, key[2])]
        assert len(image) == size
        assert image == expected[(width or 256, height or 256)], 'embedded icon differs from artwork'
    print(f'PASS: Windows x64 PE contains all {count} matching icon images: {path}')


if __name__ == '__main__':
    if len(sys.argv) == 3 and sys.argv[1] == '--exe':
        check_exe(Path(sys.argv[2]))
    else:
        unittest.main()
