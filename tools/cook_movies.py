#!/usr/bin/env python3
"""Convert already dumped PSS movies to native JPEG/PCM packets; no ISO access."""
import argparse
import hashlib
import json
from pathlib import Path
import struct
import subprocess


def demux_pcm(path):
    data = path.read_bytes()
    audio = bytearray()
    p = 0
    while p < len(data):
        if data[p:p+3] != b'\x00\x00\x01':
            raise ValueError(f'invalid PSS packet at {p}')
        kind = data[p+3]
        if kind == 0xB9:
            break
        if kind == 0xBA:
            p += 14 + (data[p+13] & 7)
            continue
        size = int.from_bytes(data[p+4:p+6], 'big')
        packet = data[p+6:p+6+size]
        if len(packet) != size:
            raise ValueError('truncated PSS packet')
        if kind == 0xBD:
            header = 3 + packet[2]
            if packet[header:header+4] != b'\xff\xa0\x00\x00':
                raise ValueError('unsupported PSS audio substream')
            audio.extend(packet[header+4:])
        p += 6 + size
    if audio[:8] != b'SShd\x18\x00\x00\x00' or audio[32:36] != b'SSbd':
        raise ValueError('missing Sony audio stream header')
    codec, rate, channels, block = struct.unpack_from('<4I', audio, 8)
    size = struct.unpack_from('<I', audio, 36)[0]
    if codec != 1 or channels != 2 or not 8000 <= rate <= 96000 or not block or block % 2 or size % (channels * block):
        raise ValueError('unsupported PCM audio layout')
    if len(audio) != size + 40:
        raise ValueError('PSS audio size differs from its header')
    pcm = bytearray(size)
    # PS2 PCM stores one block per channel. Preserve every original sample.
    for at in range(0, size, block*channels):
        left = audio[40+at:40+at+block]
        right = audio[40+at+block:40+at+block*2]
        for i in range(0, block, 2):
            pcm[at+i*2:at+i*2+4] = left[i:i+2] + right[i:i+2]
    return rate, channels, pcm


def cook(path, output, ffmpeg, ffprobe):
    probe = json.loads(subprocess.check_output([ffprobe, '-v', 'error', '-select_streams', 'v:0', '-show_entries', 'stream=width,height,r_frame_rate', '-of', 'json', str(path)]))['streams'][0]
    num, den = map(int, probe['r_frame_rate'].split('/'))
    rate, channels, pcm = demux_pcm(path)
    partial = output.with_suffix('.partial')
    partial.parent.mkdir(parents=True, exist_ok=True)
    if output.exists() or partial.exists():
        raise FileExistsError(output)
    count = 0
    command = [ffmpeg, '-v', 'error', '-i', str(path), '-map', '0:v:0', '-an', '-fps_mode', 'passthrough', '-threads', '2', '-c:v', 'mjpeg', '-q:v', '2', '-f', 'image2pipe', '-']
    try:
        with partial.open('wb') as file, subprocess.Popen(command, stdout=subprocess.PIPE) as process:
            file.write(b'RVV1' + struct.pack('<8I', probe['width'], probe['height'], num, den, 0, rate, channels, len(pcm)//(channels*2)))
            file.write(pcm)
            pending = bytearray()
            while chunk := process.stdout.read(65536):
                pending.extend(chunk)
                while (end := pending.find(b'\xff\xd9')) >= 0:
                    packet = pending[:end+2]
                    if packet[:2] != b'\xff\xd8':
                        process.kill()
                        raise ValueError('invalid JPEG pipe packet')
                    file.write(struct.pack('<I', len(packet)))
                    file.write(packet)
                    del pending[:end+2]
                    count += 1
            if process.wait() or pending or not count:
                raise RuntimeError('movie conversion failed')
            file.seek(20)
            file.write(struct.pack('<I', count))
        video_seconds = count * den / num
        audio_seconds = len(pcm)/(channels*2*rate)
        if abs(video_seconds-audio_seconds) > 5.0:
            raise ValueError(f'video/audio duration mismatch: {video_seconds}, {audio_seconds}')
        partial.replace(output)
    except BaseException:
        partial.unlink(missing_ok=True)
        raise
    return {'source': path.name, 'source_sha256': hashlib.sha256(path.read_bytes()).hexdigest(), 'frames': count, 'fps': [num, den], 'audio_frames': len(pcm)//(channels*2), 'rate': rate, 'seconds': video_seconds}


def main():
    p = argparse.ArgumentParser(description=__doc__)
    p.add_argument('--data', type=Path, default=Path('data'))
    p.add_argument('--ffmpeg', default='ffmpeg')
    p.add_argument('--ffprobe', default='ffprobe')
    p.add_argument('--movie', type=int, help='Cook one movie; default: all dumped movies')
    args = p.parse_args()
    records = []
    paths = sorted((args.data / 'files/MOVIE').glob('MV_*.PSS'))
    for path in paths:
        number = int(path.stem.split('_')[1])
        if args.movie is not None and args.movie != number:
            continue
        out = args.data / 'movies' / f'{number:04}.rvv'
        if out.exists():
            print(f'Already cooked: {out}', flush=True)
            continue
        record = cook(path, out, args.ffmpeg, args.ffprobe)
        records.append(record)
        print(f'{out}: {record["frames"]} frames, original stereo PCM, {record["seconds"]:.3f}s', flush=True)
        (out.with_suffix('.json')).write_text(json.dumps(record, indent=2)+'\n')
    if not paths:
        p.error('no dumped movie files found')


if __name__ == '__main__':
    main()
