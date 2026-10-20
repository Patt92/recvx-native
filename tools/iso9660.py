#!/usr/bin/env python3
"""Extract PlayStation 2 ISO9660 images at build time.

Only this build helper reads the container. The runtime keeps using the loose
cooked tree in data/; no ISO or AFS access is added to the game.
"""
import argparse
import hashlib
from pathlib import Path
import struct
import sys

SECTOR_LAYOUTS = ((2048, 0), (2352, 16), (2336, 8))
MAX_DIRECTORY_BYTES = 64 * 1024 * 1024
MAX_DEPTH = 32
CHUNK = 4 * 1024 * 1024


class IsoError(RuntimeError):
    pass


def _u16(data, offset):
    little = struct.unpack_from('<H', data, offset)[0]
    big = struct.unpack_from('>H', data, offset + 2)[0]
    if little != big:
        raise IsoError('invalid both-endian 16-bit field in ISO metadata')
    return little


def _u32(data, offset):
    little = struct.unpack_from('<I', data, offset)[0]
    big = struct.unpack_from('>I', data, offset + 4)[0]
    if little != big:
        raise IsoError('invalid both-endian 32-bit field in ISO metadata')
    return little


def _record(data, offset):
    if offset >= len(data):
        raise IsoError('truncated ISO directory')
    length = data[offset]
    if length == 0:
        return None
    if length < 34 or offset + length > len(data):
        raise IsoError('invalid ISO directory record')
    record = data[offset:offset + length]
    lba = _u32(record, 2)
    size = _u32(record, 10)
    flags = record[25]
    if record[1]:
        raise IsoError('extended ISO attributes are not supported')
    if record[26] or record[27]:
        raise IsoError('interleaved ISO records are not supported')
    name_length = record[32]
    if 33 + name_length > len(record):
        raise IsoError('invalid ISO directory name')
    return length, lba, size, flags, record[33:33 + name_length]


def _name(raw):
    if raw in (b'\x00', b'\x01'):
        return None
    try:
        name = raw.decode('ascii')
    except UnicodeDecodeError as error:
        raise IsoError(f'non-ASCII ISO file name: {raw!r}') from error
    name = name.split(';', 1)[0]
    if name.endswith('.'):
        name = name[:-1]
    if not name or name in ('.', '..') or any(c in name for c in '/\\:\x00'):
        raise IsoError(f'unsafe ISO file name: {name!r}')
    return name


class IsoEntry:
    __slots__ = ('name', 'extents', 'size')

    def __init__(self, name, extents):
        self.name = name
        self.extents = tuple(extents)
        self.size = sum(size for _, size in self.extents)


class IsoImage:
    def __init__(self, path):
        self.path = Path(path)
        try:
            self._stream = self.path.open('rb')
        except OSError as error:
            raise IsoError(f'cannot open ISO image: {self.path}') from error
        try:
            self._stream.seek(0, 2)
            self.size = self._stream.tell()
            self._volume()
            self.files = {}
            self.total_bytes = 0
            self._directory(self._root_lba, self._root_size, '', 0)
        except Exception:
            self._stream.close()
            raise

    def __enter__(self):
        return self

    def __exit__(self, *_):
        self.close()

    def close(self):
        self._stream.close()

    def _read_at(self, offset, size):
        if offset < 0 or size < 0 or offset + size > self.size:
            raise IsoError(f'ISO extent outside image: {offset}+{size}')
        self._stream.seek(offset)
        data = self._stream.read(size)
        if len(data) != size:
            raise IsoError(f'truncated ISO image: {self.path}')
        return data

    def _volume(self):
        head = self._read_at(0, min(self.size, 1024 * 1024))
        candidates = []
        position = head.find(b'CD001')
        while position >= 0:
            if position >= 1 and head[position - 1] == 1:
                for sector_size, data_offset in SECTOR_LAYOUTS:
                    delta = position - 1 - data_offset
                    if delta >= 0 and delta % sector_size == 0:
                        sector = delta // sector_size
                        if 16 <= sector <= 166:
                            candidates.append((sector, sector_size, data_offset))
            position = head.find(b'CD001', position + 1)
        if not candidates:
            raise IsoError(f'not an ISO9660 disc image: {self.path}')
        for sector, sector_size, data_offset in sorted(
                candidates, key=lambda candidate: (candidate[0] != 16, candidate[0], candidate[1])):
            pvd = self._read_at(sector * sector_size + data_offset, sector_size)
            try:
                if pvd[0] != 1 or pvd[1:6] != b'CD001' or pvd[6] != 1 or _u16(pvd, 128) != 2048:
                    continue
                root = _record(pvd, 156)
                if root is None:
                    continue
                _, root_lba, root_size, flags, _ = root
                if not flags & 2:
                    continue
            except IsoError:
                continue
            self._sector_size = sector_size
            self._data_offset = data_offset
            self._sector_bias = sector - 16
            self._block_size = 2048
            self.label = pvd[40:72].decode('ascii', 'replace').strip()
            self._root_lba = root_lba
            self._root_size = root_size
            return
        raise IsoError(f'not a supported ISO9660 image: {self.path}')

    def _check_extent(self, lba, size):
        offset = self._physical(lba)
        if offset < 0 or size < 0 or offset + size > self.size:
            raise IsoError(f'ISO extent outside image: {lba}+{size}')

    def _physical(self, lba):
        return (lba + self._sector_bias) * self._sector_size + self._data_offset

    def _directory(self, lba, size, prefix, depth):
        if depth > MAX_DEPTH:
            raise IsoError(f'ISO directory nesting is too deep: {prefix}')
        if size > MAX_DIRECTORY_BYTES:
            raise IsoError(f'ISO directory is too large: {prefix or "/"}')
        data = self._read_at(self._physical(lba), size)
        position = 0
        pending = None
        while position < len(data):
            if data[position] == 0:
                position = (position // self._block_size + 1) * self._block_size
                continue
            length, child_lba, child_size, flags, raw = _record(data, position)
            position += length
            name = _name(raw)
            if name is None:
                continue
            if flags & 2:
                if flags & 0x80:
                    raise IsoError(f'multi-extent ISO directory is not supported: {prefix + name}')
                self._directory(child_lba, child_size, prefix + name + '/', depth + 1)
                continue
            extents = [(child_lba, child_size)]
            if flags & 0x80:
                if pending is not None and pending[0] != name:
                    raise IsoError(f'multi-extent ISO file is not contiguous: {prefix + name}')
                if pending is None:
                    pending = (name, [])
                pending[1].append((child_lba, child_size))
                continue
            if pending is not None:
                if pending[0] != name:
                    raise IsoError(f'multi-extent ISO file is not contiguous: {prefix + pending[0]}')
                extents = pending[1]
                extents.append((child_lba, child_size))
                pending = None
            full = prefix + name
            if full in self.files:
                raise IsoError(f'duplicate ISO path: {full}')
            entry = IsoEntry(full, extents)
            for extent, extent_size in entry.extents:
                self._check_extent(extent, extent_size)
            self.files[full] = entry
            self.total_bytes += entry.size
        if pending is not None:
            raise IsoError(f'unterminated multi-extent ISO file: {prefix + pending[0]}')

    def chunks(self, entry):
        for lba, size in entry.extents:
            offset = self._physical(lba)
            self._stream.seek(offset)
            remaining = size
            while remaining:
                data = self._stream.read(min(remaining, CHUNK))
                if not data:
                    raise IsoError(f'truncated ISO file: {entry.name}')
                remaining -= len(data)
                yield data


def _digest(image, entry):
    value = hashlib.sha256()
    for data in image.chunks(entry):
        value.update(data)
    return value.digest()


def extract(images, destination):
    """Materialize one or more ISO images as a flat extracted disc tree."""
    destination = Path(destination)
    if destination.exists():
        if not destination.is_dir() or any(destination.iterdir()):
            raise IsoError(f'extraction destination is not empty: {destination}')
    else:
        destination.mkdir(parents=True)
    opened = []
    try:
        for path in images:
            opened.append(IsoImage(path))
        owners = {}
        for image in opened:
            for name, entry in image.files.items():
                if name in owners:
                    previous_image, previous_entry = owners[name]
                    if previous_entry.size != entry.size or _digest(previous_image, previous_entry) != _digest(image, entry):
                        raise IsoError(f'conflicting disc files for {name}: '
                                       f'{previous_image.path} and {image.path}')
                else:
                    owners[name] = (image, entry)
        written = 0
        total = 0
        for image in opened:
            print(f'Extracting ISO {image.path}: {image.label}, '
                  f'{len(image.files)} files / {image.total_bytes} bytes', flush=True)
            for name, entry in image.files.items():
                target = destination.joinpath(*name.split('/'))
                if target.exists():
                    continue
                target.parent.mkdir(parents=True, exist_ok=True)
                with target.open('wb') as output:
                    for data in image.chunks(entry):
                        output.write(data)
                written += 1
                total += entry.size
        return written, total, len(opened)
    finally:
        for image in opened:
            image.close()


def main(argv=None):
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('images', nargs='+', type=Path, help='PS2 ISO9660 disc image(s)')
    parser.add_argument('--list', action='store_true', help='print the ISO file tree')
    parser.add_argument('--extract', type=Path, help='extract into a new or empty directory')
    arguments = parser.parse_args(argv)
    if not arguments.list and arguments.extract is None:
        parser.error('choose --list and/or --extract')
    if arguments.list:
        for path in arguments.images:
            with IsoImage(path) as image:
                print(f'{path}: {image.label} ({len(image.files)} files, {image.total_bytes} bytes)')
                for name, entry in sorted(image.files.items()):
                    print(f'{entry.size:12d}  {name}')
    if arguments.extract is not None:
        files, total, count = extract(arguments.images, arguments.extract)
        print(f'Extracted {files} files / {total} bytes from {count} image(s) to {arguments.extract}.')


if __name__ == '__main__':
    try:
        main()
    except IsoError as error:
        print(f'ISO extraction error: {error}', file=sys.stderr)
        raise SystemExit(1)
