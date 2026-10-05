#!/usr/bin/env python3
"""Build, test and optionally package the native port using the loose data tree."""
import argparse
import os
from pathlib import Path
import platform
import subprocess
import sys

ROOT = Path(__file__).resolve().parents[1]
VERSION = '0.5'


def run(*args):
    subprocess.run([str(a) for a in args], cwd=ROOT, check=True)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--platform', choices=['host', 'macos', 'windows'], default='host')
    parser.add_argument('--jobs', type=int, default=min(os.cpu_count() or 4, 8))
    parser.add_argument('--data', type=Path, default=ROOT / 'data')
    parser.add_argument('--package', action='store_true')
    parser.add_argument('--output', type=Path, help='New package directory; refuses overwrites')
    parser.add_argument('--verify-data', action='store_true')
    parser.add_argument('--no-test', action='store_true')
    args = parser.parse_args()
    if not 1 <= args.jobs <= 256:
        parser.error('--jobs must be between 1 and 256')
    target = args.platform
    if target == 'host':
        target = {'Darwin': 'macos', 'Windows': 'windows', 'Linux': 'linux'}.get(platform.system())
    if not target:
        parser.error('unsupported host')
    data = args.data.resolve()
    if target == 'windows' and platform.system() != 'Windows':
        run(sys.executable, ROOT / 'tools/build_windows.py', '--jobs', args.jobs)
        build = ROOT / 'build/windows-x64'
        # Cross tests need Wine; run them explicitly on the target or with Wine.
        print('Cross-built Windows x64. Run the tests on Windows or via Wine.', flush=True)
    else:
        if target == 'macos' and platform.system() != 'Darwin':
            parser.error('macOS builds require a macOS host')
        build = ROOT / 'build' / ('macos-universal' if target == 'macos' else 'windows-native' if target == 'windows' else 'native')
        configure = ['cmake', '-S', ROOT, '-B', build, '-DCMAKE_BUILD_TYPE=Release', '-DRECVX_FETCH_SDL2=ON', '-DBUILD_TESTING=ON']
        if target == 'macos':
            configure += ['-DCMAKE_OSX_ARCHITECTURES=arm64;x86_64', '-DCMAKE_OSX_DEPLOYMENT_TARGET=11.0']
        run(*configure)
        run('cmake', '--build', build, '--config', 'Release', '--parallel', args.jobs)
        if not args.no_test:
            run('ctest', '--test-dir', build, '-C', 'Release', '--output-on-failure')
    binary_dir = build / 'Release' if (build / 'Release').is_dir() else build
    if args.verify_data:
        run(sys.executable, ROOT / 'tools/prepare_native_data.py', '--output', data, '--verify')
    if target != 'windows' or platform.system() == 'Windows':
        suffix = '.exe' if target == 'windows' else ''
        if not args.no_test and (data / 'native-data.version').is_file():
            run(binary_dir / ('recvx-audio-tests' + suffix), data)
            run(binary_dir / ('recvx-game-tests' + suffix), data)
            run(binary_dir / ('recvx-inventory-tests' + suffix), data)
            run(binary_dir / ('recvx-input-tests' + suffix), data)
            run(binary_dir / ('recvx-combat-tests' + suffix), data)
            run(sys.executable, ROOT / 'tools/verify_combat.py', '--runtime', binary_dir / ('recvx-runtime' + suffix), '--data', data)
            run(binary_dir / ('recvx-frontend-tests' + suffix), data)
            run(sys.executable, ROOT / 'tools/verify_frontend.py', '--runtime', binary_dir / ('recvx-runtime' + suffix), '--data', data)
            run(binary_dir / ('recvx-door-tests' + suffix), data)
            run(sys.executable, ROOT / 'tools/verify_doors.py', '--runtime', binary_dir / ('recvx-runtime' + suffix), '--data', data)
    if args.package:
        if target == 'linux':
            parser.error('the packaged targets are Windows and macOS')
        output = args.output or ROOT / 'build/packages' / (f'macos-universal-v{VERSION}' if target == 'macos' else f'windows-x64-v{VERSION}')
        run(sys.executable, ROOT / 'tools/package_native.py', '--platform', target, '--build', binary_dir, '--data', data, '--output', output)
    print(f'Build ready: {binary_dir}', flush=True)


if __name__ == '__main__':
    main()
