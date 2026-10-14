#!/usr/bin/env python3
"""Convert the application artwork to checked-in native icons (requires Pillow)."""
import argparse
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--source', type=Path, default=ROOT / 'data/app-resources/app-icon.jpg')
    parser.add_argument('--output', type=Path, default=ROOT / 'native/resources')
    args = parser.parse_args()
    from PIL import Image
    with Image.open(args.source) as source:
        if source.width != source.height:
            raise RuntimeError('application artwork must be square; refusing to crop it')
        image = source.convert('RGB').resize((1024, 1024), Image.Resampling.LANCZOS)
    args.output.mkdir(parents=True, exist_ok=True)
    image.save(args.output / 'app-icon.icns', format='ICNS')
    image.save(args.output / 'app-icon.ico', format='ICO',
               sizes=[(size, size) for size in (16, 24, 32, 48, 64, 128, 256)])
    print(f'Application icons: {args.output}')


if __name__ == '__main__':
    main()
