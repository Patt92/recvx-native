#!/usr/bin/env python3
"""Snapshot keeps the room sound pack of a room variant (host field, format 2).

sdfunc.c:749 picks the SPQ pack by stage*1000 + room*10 + CaseNo, where CaseNo
comes from bhRoomSoundCase (0xB6) in the source room's exit scenario. RM_2011's
zombie SE bank lives only in pack 2011; a direct start (case 0) loads pack 2010
and every zombie SE is outside the TSNDDRV table. A snapshot written in RM_2011
carries the loaded case so that loading it restores pack 2011.
Only the case byte of a runtime-written snapshot is patched here; the file
format (format word, CRC32) is the one of native/snapshot.cpp.
"""
import argparse
import json
import struct
import zlib
from pathlib import Path
import subprocess
import tempfile


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--runtime', type=Path, required=True)
    parser.add_argument('--data', type=Path, required=True)
    parser.add_argument('--output', type=Path)
    args = parser.parse_args()
    runtime, data = args.runtime.resolve(), args.data.resolve()
    with tempfile.TemporaryDirectory(prefix='recvx-snapsound-') as temporary:
        root = Path(temporary)
        out = args.output.resolve() if args.output else root
        out.mkdir(parents=True, exist_ok=True)
        controls = root / 'controls.ini'
        subprocess.run([str(runtime), '--write-controls', str(controls)], check=True, stdout=subprocess.DEVNULL)

        def run(name, frames, lines, snapshots):
            script, state = out / (name + '.txt'), out / (name + '.json')
            script.write_text('\n'.join(lines) + '\n')
            state.unlink(missing_ok=True)
            command = [str(runtime), '--data', str(data), '--mods', str(root / 'no-mods'),
                       '--user-dir', str(root / 'user'), '--controls', str(controls),
                       '--debug-config', str(root / 'debug.ini'), '--snapshots', str(snapshots),
                       '--in-game', '--room', 'RM_2011.RDX', '--renderer', 'software', '--enemy-ai', 'on',
                       '--headless', '--frames', str(frames), '--replay', str(script), '--state', str(state)]
            with (out / (name + '.log')).open('w') as log:
                subprocess.run(command, check=True, stdout=log, stderr=subprocess.STDOUT, timeout=120)
            return json.loads(state.read_text())

        slots = root / 'slots'
        slots.mkdir()
        run('save', 30, ['5 debug_key F2', '6 debug_select snapshot_save 0', '7 confirm'], slots)
        path = slots / 'snapshot-1.rcvxsnap'
        raw = bytearray(path.read_bytes())
        assert raw[:8] == b'RCVXSNAP' and struct.unpack_from('<I', raw, 8)[0] == 2, 'runtime did not write a format 2 snapshot'
        body = raw[:-4]
        assert zlib.crc32(bytes(body)) == struct.unpack_from('<I', raw, len(raw) - 4)[0], 'snapshot CRC differs from zlib crc32'
        assert body[-1] == 0, 'direct start must record room sound case 0'

        direct = run('direct', 600, ['0 debug_key F2', '1 debug_select snapshot_load 0', '2 confirm'], slots)
        missing = [m for m in direct['audio_unavailable'] if 'SE bank 3' in m]
        assert missing, 'counter-check: case 0 must still lack the zombie bank (pack 2010)'

        body[-1] = 1
        patched = body + struct.pack('<I', zlib.crc32(bytes(body)))
        variant = root / 'variant'
        variant.mkdir()
        (variant / 'snapshot-1.rcvxsnap').write_bytes(patched)
        loaded = run('variant', 600, ['0 debug_key F2', '1 debug_select snapshot_load 0', '2 confirm'], variant)
        assert loaded['room'] == 'RM_2011.RDX' and not loaded['debug_open'], loaded['room']
        bank3 = [m for m in loaded['audio_unavailable'] if 'SE bank 3' in m]
        assert not bank3, ('variant pack not restored', bank3[:3])
        played = [p for p in loaded['audio_played'] if p.startswith('MULTSPQ1/0039/3/')]
        assert played, ('no zombie SE from pack 2011 (0039) played', loaded['audio_played'][:6])
        print(f'Snapshot room sound case 1 restores pack 2011: {len(played)} bank-3 SE played, none missing; case 0 counter-check missing {len(missing)}.')


if __name__ == '__main__':
    main()
