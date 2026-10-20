#!/usr/bin/env python3
"""Build and optionally verify or package the native port using the loose data tree."""
import argparse
import os
from pathlib import Path
import platform
import plistlib
import shlex
import shutil
import re
import subprocess
import sys
import tempfile

ROOT = Path(__file__).resolve().parents[1]
def _project_version():
    # Einzige Quelle: project(... VERSION x.y.z) in CMakeLists.txt; die Buildnummer
    # zaehlt die Commits (git rev-list --count HEAD), damit jede App ihre Version traegt.
    text = (Path(__file__).resolve().parents[1] / 'CMakeLists.txt').read_text()
    major, minor = re.search(r'project\(recvx_native VERSION (\d+)\.(\d+)', text).groups()
    try:
        build = subprocess.check_output(['git', 'rev-list', '--count', 'HEAD'], cwd=Path(__file__).resolve().parents[1], text=True).strip()
    except (OSError, subprocess.CalledProcessError):
        build = '0'
    return f'{major}.{minor}', f'{major}.{minor}.{build}'


VERSION, FULL_VERSION = _project_version()  # VERSION benennt das Paketverzeichnis (Start.command)


def run(*args):
    subprocess.run([str(a) for a in args], cwd=ROOT, check=True)


def sync_macos_start_package(build: Path) -> None:
    """Create a verified development app, independently of retired packages.

    Development uses the current loose data and settings in the project. The
    separate --package workflow still creates standalone copies of the data.
    No files are restored from a retired app or from quarantine.
    """
    source = build / 'recvx-runtime'
    if not source.is_file():
        raise RuntimeError(f'cannot create development app: missing {source}')
    if subprocess.check_output(['lipo', '-archs', str(source)], text=True).split() != ['arm64']:
        raise RuntimeError('macOS starter package requires an arm64-only runtime')
    dependencies = subprocess.check_output(['otool', '-L', str(source)], text=True)
    linked = next(line.strip().split(' (', 1)[0] for line in dependencies.splitlines()[1:] if 'libSDL2' in line)
    library_name = Path(linked).name
    library = build / '_deps/sdl2-build' / library_name
    if not library.is_file():
        raise RuntimeError(f'development app requires the built SDL2 library: {library}')
    if subprocess.check_output(['lipo', '-archs', str(library.resolve())], text=True).split() != ['arm64']:
        raise RuntimeError('macOS starter requires an arm64-only SDL2 library')
    parent = ROOT / 'build/start/macos-arm64'
    parent.mkdir(parents=True, exist_ok=True)
    app = parent / 'RECVX Native.app'
    if app.is_symlink():
        raise RuntimeError('refusing to replace a symlinked development app')
    with tempfile.TemporaryDirectory(prefix='.staging-', dir=parent) as temporary:
        candidate = Path(temporary) / app.name
        contents = candidate / 'Contents'
        binaries = contents / 'MacOS'
        frameworks = contents / 'Frameworks'
        binaries.mkdir(parents=True)
        frameworks.mkdir()
        runtime = binaries / 'recvx-runtime'
        bundled_library = frameworks / library_name
        shutil.copy2(source, runtime)
        shutil.copy2(library.resolve(), bundled_library)
        run('install_name_tool', '-change', linked, '@executable_path/../Frameworks/' + library_name, runtime)
        run('install_name_tool', '-id', '@rpath/' + library_name, bundled_library)
        launcher = binaries / 'Start'
        options = ' '.join(shlex.quote(str(value)) for value in (
            '--data', ROOT / 'data', '--mods', ROOT / 'mods',
            '--controls', ROOT / 'controls.ini', '--debug-config', ROOT / 'debug.ini'))
        launcher.write_text('#!/bin/sh\nrecvx_dir=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)\n'
                            f'exec "$recvx_dir/recvx-runtime" {options} "$@"\n')
        launcher.chmod(0o755)
        resources = contents / 'Resources'
        resources.mkdir()
        shutil.copy2(ROOT / 'native/resources/app-icon.icns', resources / 'app-icon.icns')
        with (contents / 'Info.plist').open('wb') as info:
            plistlib.dump({'CFBundleExecutable': 'Start', 'CFBundleIdentifier': 'org.recvx.native.development',
                          'CFBundleName': 'RECVX Native', 'CFBundlePackageType': 'APPL',
                          'CFBundleIconFile': 'app-icon.icns',
                          'CFBundleVersion': FULL_VERSION, 'CFBundleShortVersionString': FULL_VERSION, 'LSMinimumSystemVersion': '11.0',
                          'NSHighResolutionCapable': True}, info)
        run(sys.executable, ROOT / 'tools/check_no_original_core.py', ROOT, '--binary', runtime)
        run('codesign', '--force', '--sign', '-', bundled_library)
        run('codesign', '--force', '--sign', '-', runtime)
        run('codesign', '--force', '--sign', '-', candidate)
        run('codesign', '--verify', '--deep', '--strict', candidate)
        # Publish only after all checks; retain the previous development app.
        backup = None
        if app.exists():
            backup = Path(tempfile.mkdtemp(prefix='.previous-', dir=parent)) / app.name
            app.rename(backup)
        try:
            candidate.rename(app)
        except OSError:
            if backup is not None:
                backup.rename(app)
            raise
    print(f'Updated Start.command development app: {app}', flush=True)


def prepare_iso_data(images, data: Path, binary_dir: Path, target: str, jobs: int) -> None:
    """Create the complete loose data tree from ISO images only when none exists yet."""
    if (data / 'native-data.version').is_file():
        print(f'Using existing prepared data at {data}; --iso is not extracted again.', flush=True)
        return
    if data.exists():
        raise RuntimeError(f'--iso needs a new data directory, but {data} exists without a completed export')
    if target == 'windows' and platform.system() != 'Windows':
        # The exporters run on the build host, not on the cross-compiled target.
        directories = [ROOT / 'build/macos-arm64', ROOT / 'build/native']
        suffix = ''
    else:
        directories = [binary_dir]
        suffix = '.exe' if target == 'windows' else ''
    names = ('recvx-assets' + suffix, 'recvx-world-assets' + suffix)
    directory = next((d for d in directories if all((d / n).is_file() for n in names)), None)
    if directory is None:
        raise RuntimeError('ISO extraction requires the built host exporters '
                           f'{" and ".join(names)} in {", ".join(map(str, directories))}')
    exporter = directory / names[0]
    command = [sys.executable, str(ROOT / 'tools/prepare_native_data.py'),
               '--output', str(data), '--exporter', str(exporter), '--jobs', str(jobs)]
    for image in images:
        command += ['--iso', str(image.absolute())]  # run() starts in ROOT, not in the caller's directory
    run(*command)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--platform', choices=['host', 'macos', 'windows'], default='host')
    parser.add_argument('--jobs', type=int, default=min(os.cpu_count() or 4, 8))
    parser.add_argument('--data', type=Path, default=ROOT / 'data')
    parser.add_argument('--iso', type=Path, action='append',
                        help='PS2 ISO9660 image used to create missing --data; repeat to merge discs')
    parser.add_argument('--package', action='store_true')
    parser.add_argument('--output', type=Path, help='New package directory; refuses overwrites')
    parser.add_argument('--verify-data', action='store_true')
    parser.add_argument('--test', action='store_true', help='Build test targets and run CTest')
    parser.add_argument('--verify-runtime', action='store_true', help='Run the opt-in original-data runtime regression suite')
    parser.add_argument('--no-test', action='store_true', help=argparse.SUPPRESS)
    args = parser.parse_args()
    run(sys.executable, ROOT / 'tools/check_no_original_core.py', ROOT)
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
    run_checks = args.test and not args.no_test
    if target == 'windows' and platform.system() != 'Windows':
        # The final binary audit also invokes Zig; use the same project cache.
        os.environ['ZIG_GLOBAL_CACHE_DIR'] = str(ROOT / 'build/toolchains/zig-cache')
        run(sys.executable, ROOT / 'tools/build_windows.py', '--jobs', args.jobs)
        build = ROOT / 'build/windows-x64'
        print('Cross-built Windows x64.', flush=True)
    else:
        if target == 'macos' and platform.system() != 'Darwin':
            parser.error('macOS builds require a macOS host')
        build = ROOT / 'build' / ('macos-arm64' if target == 'macos' else 'windows-native' if target == 'windows' else 'native')
        configure = ['cmake', '-S', ROOT, '-B', build, '-DCMAKE_BUILD_TYPE=Release', '-DRECVX_FETCH_SDL2=ON', f'-DBUILD_TESTING={"ON" if run_checks or args.verify_runtime else "OFF"}', f'-DRECVX_BUILD_VERSION={FULL_VERSION}']
        if target == 'macos':
            configure += ['-DCMAKE_OSX_ARCHITECTURES=arm64', '-DCMAKE_OSX_DEPLOYMENT_TARGET=11.0']
        run(*configure)
        run('cmake', '--build', build, '--config', 'Release', '--parallel', args.jobs)
    binary_dir = build / 'Release' if (build / 'Release').is_dir() else build
    suffix = '.exe' if target == 'windows' else ''
    if args.iso:
        prepare_iso_data(args.iso, data, binary_dir, target, args.jobs)
    if args.verify_runtime and not (data / 'native-data.version').is_file():
        parser.error(f'--verify-runtime requires a prepared native data tree (missing {data / "native-data.version"})')
    if run_checks:
        run('ctest', '--test-dir', build, '-C', 'Release', '--output-on-failure')
    run(sys.executable, ROOT / 'tools/check_no_original_core.py', ROOT,
        '--build-dir', build, '--binary', binary_dir / ('recvx-runtime' + suffix))
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
    if target == 'macos':
        sync_macos_start_package(binary_dir)
    print(f'Build ready: {binary_dir}', flush=True)


if __name__ == '__main__':
    main()
