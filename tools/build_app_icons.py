#!/usr/bin/env python3
"""Convert the application artwork to checked-in native icons (requires Pillow)."""
import argparse
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
# iOS bundle icons (CFBundleIconFiles in tools/build_native.py): iPhone 60 pt @2x/@3x, iPad 76/83.5 pt @2x.
IOS_ICONS = {'AppIcon60x60@2x.png': 120, 'AppIcon60x60@3x.png': 180,
             'AppIcon76x76@2x~ipad.png': 152, 'AppIcon83.5x83.5@2x~ipad.png': 167}
# tvOS: flaches App-Icon 400x240 pt @1x/@2x (das geschichtete Format braucht Xcode-Assetkataloge).
TVOS_ICONS = {'AppIcon400x240.png': (400, 240), 'AppIcon400x240@2x.png': (800, 480)}
# Android-Launcher (android/app/src/main/res/mipmap-*/ic_launcher.png): 48 dp je Dichtestufe.
ANDROID_ICONS = {'mdpi': 48, 'hdpi': 72, 'xhdpi': 96, 'xxhdpi': 144, 'xxxhdpi': 192}
ANDROID_RES = ROOT / 'android/app/src/main/res'


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--source', type=Path, default=ROOT / 'data/app-resources/app-icon.jpg')
    parser.add_argument('--output', type=Path, default=ROOT / 'native/resources')
    parser.add_argument('--android-res', type=Path, default=ANDROID_RES)
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
    (args.output / 'ios').mkdir(exist_ok=True)
    for name, size in IOS_ICONS.items():
        image.resize((size, size), Image.Resampling.LANCZOS).save(args.output / 'ios' / name, format='PNG')
    (args.output / 'tvos').mkdir(exist_ok=True)
    for name, (width, height) in TVOS_ICONS.items():
        # Das quadratische Motiv mittig in das 5:3-Feld, Rand in der Eckfarbe des Motivs.
        corner = image.getpixel((0, 0))
        canvas = Image.new('RGB', (width, height), corner)
        side = height
        canvas.paste(image.resize((side, side), Image.Resampling.LANCZOS), ((width - side) // 2, 0))
        canvas.save(args.output / 'tvos' / name, format='PNG')
    for density, size in ANDROID_ICONS.items():
        folder = args.android_res / f'mipmap-{density}'
        folder.mkdir(parents=True, exist_ok=True)
        image.resize((size, size), Image.Resampling.LANCZOS).save(folder / 'ic_launcher.png', format='PNG')
    print(f'Application icons: {args.output}')


if __name__ == '__main__':
    main()
