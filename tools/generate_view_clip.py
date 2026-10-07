#!/usr/bin/env python3
"""Extract the original njClipZ tables of bhChangeViewClip (event cameras) and
bhChangeViewClipRM (room cameras) from event.c. Values stay as written there."""
import re
import sys
from pathlib import Path

ROW = re.compile(r'\{\s*/\*\s*room\s*\*/\s*(-?\d+),\s*/\*\s*rcase\s*\*/\s*(-?\d+),\s*/\*\s*evc_no\s*\*/\s*(-?\d+),'
                 r'\s*/\*\s*near\s*\*/\s*(-?[\d.]+)f?,\s*/\*\s*far\s*\*/\s*(-?[\d.]+)f?\s*\}')


def tables(source, function, prefix):
    start = source.index('void ' + function + '(')
    end = source.index('njClipZ', start)
    body = source[start:end]
    out, names = [], []
    for stage in range(10):
        m = re.search(r'static VIEW_CLIP ViewClipSt%d\[(\d+)\]\s*=\s*\{(.*?)\};' % stage, body, re.S)
        if not m:
            raise SystemExit(f'{function}: ViewClipSt{stage} missing')
        rows = ROW.findall(m.group(2))
        if len(rows) != int(m.group(1)) or rows[-1][0] != '-1':
            raise SystemExit(f'{function}: ViewClipSt{stage} row count/terminator mismatch')
        name = f'{prefix}_st{stage}'
        names.append(name)
        out.append(f'static const ViewClipRow {name}[]={{' + ','.join(
            f'{{{r},{c},{e},{float(n)!r}f,{float(f)!r}f}}' for r, c, e, n, f in rows) + '};')
    out.append(f'static const ViewClipRow* const {prefix}_st[10]={{' + ','.join(names) + '};')
    return out


root, output = map(Path, sys.argv[1:])
source = (root / 'src/ps2/veronica/prog/event.c').read_text()
lines = ['// Generated from event.c bhChangeViewClip / bhChangeViewClipRM; do not edit.']
lines += tables(source, 'bhChangeViewClip', 'event_view_clip')
lines += tables(source, 'bhChangeViewClipRM', 'room_view_clip')
output.mkdir(parents=True, exist_ok=True)
(output / 'view_clip.inc').write_text('\n'.join(lines) + '\n')
