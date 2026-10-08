#!/usr/bin/env python3
"""Build and optionally verify or package the native port using the loose data tree."""
import argparse
import os
from pathlib import Path
import platform
import shutil
import subprocess
import sys

ROOT = Path(__file__).resolve().parents[1]
VERSION = '0.5'


def run(*args):
    subprocess.run([str(a) for a in args], cwd=ROOT, check=True)


def sync_macos_start_package(build: Path) -> None:
    """Refresh the existing app that the root Start.command launches.

    A normal development build must not leave Start.command pointing at a stale
    package runtime.  This deliberately replaces only the executable: cooked
    package data and user-adjacent package files are retained.
    """
    app = ROOT / 'build/packages' / f'macos-arm64-v{VERSION}' / 'RECVX Native.app'
    if not app.is_dir():
        return
    source = build / 'recvx-runtime'
    runtime = app / 'Contents/MacOS/recvx-runtime'
    if not source.is_file() or not runtime.is_file():
        raise RuntimeError(f'cannot refresh Start.command package runtime: {runtime}')
    if subprocess.check_output(['lipo', '-archs', str(source)], text=True).split() != ['arm64']:
        raise RuntimeError('macOS starter package requires an arm64-only runtime')
    shutil.copy2(source, runtime)
    dependencies = subprocess.check_output(['otool', '-L', str(source)], text=True)
    linked = next(line.strip().split(' (', 1)[0] for line in dependencies.splitlines()[1:] if 'libSDL2' in line)
    subprocess.run(['install_name_tool', '-change', linked, '@executable_path/../Frameworks/' + Path(linked).name, str(runtime)], check=True)
    subprocess.run(['codesign', '--force', '--sign', '-', str(runtime)], check=True)
    subprocess.run(['codesign', '--force', '--sign', '-', str(app)], check=True)
    subprocess.run(['codesign', '--verify', '--deep', '--strict', str(app)], check=True)
    print(f'Updated Start.command package: {app}', flush=True)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--platform', choices=['host', 'macos', 'windows'], default='host')
    parser.add_argument('--jobs', type=int, default=min(os.cpu_count() or 4, 8))
    parser.add_argument('--data', type=Path, default=ROOT / 'data')
    parser.add_argument('--package', action='store_true')
    parser.add_argument('--output', type=Path, help='New package directory; refuses overwrites')
    parser.add_argument('--verify-data', action='store_true')
    parser.add_argument('--test', action='store_true', help='Build test targets and run CTest')
    parser.add_argument('--verify-runtime', action='store_true', help='Run the opt-in original-data runtime regression suite')
    parser.add_argument('--no-test', action='store_true', help=argparse.SUPPRESS)
    args = parser.parse_args()
    if not 1 <= args.jobs <= 256:
        parser.error('--jobs must be between 1 and 256')
    target = args.platform
    if target == 'host':
        target = {'Darwin': 'macos', 'Windows': 'windows', 'Linux': 'linux'}.get(platform.system())
    if not target:
        parser.error('unsupported host')
    data = args.data.resolve()
    if target == 'windows' and platform.system() != 'Windows' and (args.test or args.verify_runtime):
        parser.error('Windows cross-builds cannot run tests; use --platform windows without test options, then test on Windows or Wine')
    if args.verify_runtime and not (data / 'native-data.version').is_file():
        parser.error(f'--verify-runtime requires a prepared native data tree (missing {data / "native-data.version"})')
    if target == 'windows' and platform.system() != 'Windows':
        run(sys.executable, ROOT / 'tools/build_windows.py', '--jobs', args.jobs)
        build = ROOT / 'build/windows-x64'
        print('Cross-built Windows x64.', flush=True)
    else:
        if target == 'macos' and platform.system() != 'Darwin':
            parser.error('macOS builds require a macOS host')
        build = ROOT / 'build' / ('macos-arm64' if target == 'macos' else 'windows-native' if target == 'windows' else 'native')
        run_checks = args.test and not args.no_test
        configure = ['cmake', '-S', ROOT, '-B', build, '-DCMAKE_BUILD_TYPE=Release', '-DRECVX_FETCH_SDL2=ON', f'-DBUILD_TESTING={"ON" if run_checks or args.verify_runtime else "OFF"}']
        if target == 'macos':
            configure += ['-DCMAKE_OSX_ARCHITECTURES=arm64', '-DCMAKE_OSX_DEPLOYMENT_TARGET=11.0']
        run(*configure)
        run('cmake', '--build', build, '--config', 'Release', '--parallel', args.jobs)
        if run_checks:
            run('ctest', '--test-dir', build, '-C', 'Release', '--output-on-failure')
    binary_dir = build / 'Release' if (build / 'Release').is_dir() else build
    if args.verify_data:
        run(sys.executable, ROOT / 'tools/prepare_native_data.py', '--output', data, '--verify')
    if target != 'windows' or platform.system() == 'Windows':
        suffix = '.exe' if target == 'windows' else ''
        if args.verify_runtime and (data / 'native-data.version').is_file():
            run(binary_dir / ('recvx-audio-tests' + suffix), data)
            run(binary_dir / ('recvx-game-tests' + suffix), data)
            run(binary_dir / ('recvx-progression-tests' + suffix), data)
            run(sys.executable, ROOT / 'tools/verify_progression.py', '--runtime', binary_dir / ('recvx-runtime' + suffix), '--data', data)
            run(sys.executable, ROOT / 'tools/verify_detector.py', '--runtime', binary_dir / ('recvx-runtime' + suffix), '--data', data)
            run(binary_dir / ('recvx-inventory-tests' + suffix), data)
            run(binary_dir / ('recvx-inventory-engine-tests' + suffix), data)
            run(binary_dir / ('recvx-debug-tests' + suffix), data)
            run(sys.executable, ROOT / 'tools/verify_debug.py', '--runtime', binary_dir / ('recvx-runtime' + suffix), '--data', data)
            run(sys.executable, ROOT / 'tools/verify_events.py', '--runtime', binary_dir / ('recvx-runtime' + suffix), '--data', data)
            run(sys.executable, ROOT / 'tools/verify_audio_state.py', '--runtime', binary_dir / ('recvx-runtime' + suffix), '--data', data)
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
        output = args.output or ROOT / 'build/packages' / (f'macos-arm64-v{VERSION}' if target == 'macos' else f'windows-x64-v{VERSION}')
        run(sys.executable, ROOT / 'tools/package_native.py', '--platform', target, '--build', binary_dir, '--data', data, '--output', output)
    elif target == 'macos':
        sync_macos_start_package(binary_dir)
    print(f'Build ready: {binary_dir}', flush=True)


if __name__ == '__main__':
    main()
