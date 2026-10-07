#!/usr/bin/env python3
"""Read-only helpers for the GameCube Code Veronica X dump (GCDE08): disc FST,
AFS archives and the shared PS2/GC 'Expand' LZ stream. Used to build exact
texture mappings; never part of the runtime."""
import struct


class GcDisc:
    def __init__(self, path):
        self.f = open(path, 'rb')
        self.f.seek(0)
        header = self.f.read(0x440)
        self.game_id = header[:6].decode('ascii')
        fst_off, fst_size = struct.unpack_from('>II', header, 0x424)
        self.f.seek(fst_off)
        fst = self.f.read(fst_size)
        count = struct.unpack_from('>I', fst, 8)[0]
        names = fst[count * 12:]
        self.files = {}

        def walk(i, end, prefix):
            while i < end:
                flag_off, a, b = struct.unpack_from('>III', fst, i * 12)
                off = flag_off & 0xffffff
                name = names[off:names.index(b'\0', off)].decode('latin1')
                if flag_off >> 24:
                    walk(i + 1, b, prefix + name + '/')
                    i = b
                else:
                    self.files[prefix + name] = (a, b)
                    i += 1
        walk(1, count, '')

    def read(self, name):
        a, b = self.files[name]
        self.f.seek(a)
        return self.f.read(b)


def afs_entries(data):
    """-> list of (name, bytes) for an AFS archive (name table at the end)."""
    assert data[:4] == b'AFS\0'
    count = struct.unpack_from('<I', data, 4)[0]
    entries = [struct.unpack_from('<II', data, 8 + i * 8) for i in range(count)]
    names = [None] * count
    table = struct.unpack_from('<II', data, 8 + count * 8)
    if table[0] and table[0] + count * 48 <= len(data):
        for i in range(count):
            names[i] = data[table[0] + i * 48:table[0] + i * 48 + 32].split(b'\0')[0].decode('latin1')
    return [(names[i] or f'{i:04d}', data[o:o + s]) for i, (o, s) in enumerate(entries)]


def expand(source, limit=1 << 28):
    """Port of native/expand.cpp (the original Expand LZ stream)."""
    pos = 0
    out = bytearray()
    remaining = 9
    descriptor = source[pos]
    pos += 1

    def bit():
        nonlocal descriptor, remaining, pos
        remaining -= 1
        if remaining == 0:
            descriptor = source[pos]
            pos += 1
            remaining = 8
        value = descriptor & 1
        descriptor >>= 1
        return value
    while True:
        if bit():
            out.append(source[pos])
            pos += 1
            continue
        if not bit():
            high = bit()
            low = bit()
            count = 2 + high * 2 + low
            distance = 256 - source[pos]
            pos += 1
        else:
            lower = source[pos]
            pos += 1
            whole = lower | (source[pos] << 8)
            pos += 1
            if whole == 0:
                return bytes(out)
            distance = 8192 - (whole >> 3)
            count = whole & 7
            if count:
                count += 2
            else:
                count = source[pos] + 1
                pos += 1
        if distance > len(out) or len(out) + count > limit:
            raise ValueError('bad Expand stream')
        for _ in range(count):
            out.append(out[-distance])
