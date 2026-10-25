#!/usr/bin/env python3
"""Build and optionally verify or package the native port using the loose data tree."""
import argparse
import hashlib
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
sys.path.insert(0, str(Path(__file__).resolve().parent))
from data_profiles import profile as data_profile  # noqa: E402
from bundle_data import stage_bundle_data, mods_with_content  # noqa: E402


def _project_version():
    # Einzige Quelle: project(... VERSION x.y.z) in CMakeLists.txt; die Buildnummer
    # zaehlt die Commits (git rev-list --count HEAD), damit jede App ihre Version traegt.
    text = (Path(__file__).resolve().parents[1] / 'CMakeLists.txt').read_text()
    major, minor = re.search(r'project\(recvx_native VERSION (\d+)\.(\d+)', text).groups()
    try:
        build = subprocess.check_output(['git', 'rev-list', '--count', 'HEAD'], cwd=Path(__file__).resolve().parents[1], text=True,
                                        stderr=subprocess.DEVNULL).strip()
    except (OSError, subprocess.CalledProcessError):
        # Ohne Repository (Docker-Builder ohne .git) kommt die Nummer aus der Umgebung.
        build = os.environ.get('RECVX_BUILD_NUMBER', '0')
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


WINDOWS_TARGETS = {'windows': 'x64', 'windows-arm64': 'arm64'}


def host_target(system: str, machine: str) -> str | None:
    """Map the host to a build target; Windows ARM64 builds natively for ARM64."""
    if system == 'Darwin':
        return 'macos'
    if system == 'Windows':
        return 'windows-arm64' if machine.lower() in {'arm64', 'aarch64'} else 'windows'
    if system == 'Linux':
        return 'linux'
    return None


def host_machine() -> str:
    """Native CPU of the host; x64 Python under emulation on Windows ARM64 reports AMD64."""
    machine = platform.machine()
    if platform.system() != 'Windows':
        return machine
    try:
        import ctypes
        process, native = ctypes.c_ushort(), ctypes.c_ushort()
        kernel = ctypes.windll.kernel32
        kernel.GetCurrentProcess.restype = ctypes.c_void_p
        kernel.IsWow64Process2.argtypes = [ctypes.c_void_p, ctypes.POINTER(ctypes.c_ushort), ctypes.POINTER(ctypes.c_ushort)]
        if kernel.IsWow64Process2(kernel.GetCurrentProcess(), ctypes.byref(process), ctypes.byref(native)):
            return {0xAA64: 'ARM64', 0x8664: 'AMD64', 0x014C: 'x86'}.get(native.value, machine)
    except (AttributeError, OSError):
        pass  # IsWow64Process2 needs Windows 10 1709+
    return machine


VS_CMAKE = 'Common7/IDE/CommonExtensions/Microsoft/CMake/CMake/bin/cmake.exe'


def visual_studio_major(folder: str) -> int:
    """Folder names are years up to 2022 (major 15..17) and major versions from 18 on."""
    years = {'2017': 15, '2019': 16, '2022': 17}
    return years.get(folder, int(folder) if folder.isdigit() and int(folder) < 100 else 0)


def find_cmake() -> str | None:
    """CMake from PATH, else the copy bundled with any Visual Studio (release, preview, Insiders) or the CMake installer."""
    found = shutil.which('cmake')
    if found or platform.system() != 'Windows':
        return found
    program_files = [Path(os.environ.get(name, default)) for name, default in
                     (('ProgramFiles', r'C:\Program Files'), ('ProgramFiles(x86)', r'C:\Program Files (x86)'))]
    candidates = []
    vswhere = program_files[1] / 'Microsoft Visual Studio/Installer/vswhere.exe'
    if vswhere.is_file():
        try:
            installs = subprocess.check_output([str(vswhere), '-all', '-prerelease', '-products', '*', '-sort',
                                                '-property', 'installationPath'], text=True, errors='replace').splitlines()
        except (OSError, subprocess.CalledProcessError):
            installs = []
        candidates += [Path(i.strip()) / VS_CMAKE for i in installs if i.strip()]
    # Without vswhere: <Program Files>/Microsoft Visual Studio/<version>/<edition>, newest version first.
    for base in program_files:
        candidates += sorted((base / 'Microsoft Visual Studio').glob('*/*/' + VS_CMAKE),
                             key=lambda c: visual_studio_major(c.parents[8].name), reverse=True)
    candidates += [base / 'CMake/bin/cmake.exe' for base in program_files]
    return next((str(c) for c in candidates if c.is_file()), None)


def visual_studio_platform(target: str) -> list[str]:
    """Pin the Visual Studio generator platform to the target instead of CMake's default."""
    generator = os.environ.get('CMAKE_GENERATOR', '')
    if platform.system() != 'Windows' or (generator and not generator.startswith('Visual Studio')):
        return []
    return ['-A', 'ARM64' if WINDOWS_TARGETS[target] == 'arm64' else 'x64']


# Ninja ships with the C++ CMake tools; older layouts keep it next to cmake.exe.
VS_NINJA = ('Common7/IDE/CommonExtensions/Microsoft/CMake/Ninja/ninja.exe',
            'Common7/IDE/CommonExtensions/Microsoft/CMake/CMake/bin/ninja.exe')
VS_VCVARSALL = 'VC/Auxiliary/Build/vcvarsall.bat'


def visual_studio_root(cmake: str) -> Path | None:
    """Installation root when `cmake` is the copy bundled with Visual Studio."""
    path = Path(cmake)
    if path.as_posix().lower().endswith(VS_CMAKE.lower()):
        return path.parents[7]
    return None


def find_ninja(cmake: str) -> str | None:
    """Ninja from PATH, else the copy Visual Studio ships next to its CMake."""
    found = shutil.which('ninja')
    if found:
        return found
    root = visual_studio_root(cmake)
    for relative in VS_NINJA if root else ():
        if (root / relative).is_file():
            return str(root / relative)
    return None


def find_vcvarsall(cmake: str) -> Path | None:
    root = visual_studio_root(cmake)
    candidates = [root / VS_VCVARSALL] if root else []
    for name, default in (('ProgramFiles', r'C:\Program Files'), ('ProgramFiles(x86)', r'C:\Program Files (x86)')):
        base = Path(os.environ.get(name, default)) / 'Microsoft Visual Studio'
        candidates += sorted(base.glob('*/*/' + VS_VCVARSALL), key=lambda c: visual_studio_major(c.parents[4].name), reverse=True)
    return next((c for c in candidates if c.is_file()), None)


def vcvars_architecture(host: str, target: str) -> str:
    """vcvarsall.bat argument: native tools when host and target agree, else the <host>_<target> cross tools."""
    host_arch = 'arm64' if host.lower() in {'arm64', 'aarch64'} else 'x64'
    target_arch = WINDOWS_TARGETS[target]
    return target_arch if host_arch == target_arch else f'{host_arch}_{target_arch}'


def parse_environment(listing: str) -> dict[str, str]:
    environment = {}
    for line in listing.splitlines():
        key, separator, value = line.partition('=')
        if separator and key and not key.startswith(' '):
            environment[key] = value.rstrip('\r')
    return environment


def msvc_environment(target: str, cmake: str) -> dict[str, str] | None:
    """Environment of the Visual Studio developer prompt for the target (cl.exe, INCLUDE, LIB)."""
    wanted = WINDOWS_TARGETS[target]
    if os.environ.get('VSCMD_ARG_TGT_ARCH', '').lower() == wanted and shutil.which('cl'):
        return dict(os.environ)
    vcvarsall = find_vcvarsall(cmake)
    if vcvarsall is None:
        return None
    architecture = vcvars_architecture(host_machine(), target)
    # One command string: an argument list would escape the inner quotes as \" for cmd.exe, which does not
    # understand them. /s strips the outer pair, leaving "<vcvarsall>" <arch> ... for cmd to run.
    command = f'cmd.exe /d /s /c ""{vcvarsall}" {architecture} >nul 2>&1 && set"'
    try:
        listing = subprocess.check_output(command, text=True, errors='replace', cwd=ROOT)
    except (OSError, subprocess.CalledProcessError) as error:
        print(f'{vcvarsall} {architecture} failed: {error}', flush=True)
        return None
    environment = parse_environment(listing)
    if 'INCLUDE' in environment and 'LIB' in environment:
        return environment
    print(f'{vcvarsall} {architecture} set no INCLUDE/LIB (toolset for {architecture} not installed?)', flush=True)
    return None


def windows_generator(target: str, cmake: str) -> tuple[list[str], dict[str, str] | None, str]:
    """Configure arguments, build environment and generator name for a native Windows build.

    Ninja with the MSVC developer environment is preferred: it runs every build command directly. The
    Visual Studio generator writes each custom build step (FetchContent population, SDL2 header
    generation, DLL copies) as a .cmd file into the build directory and runs it from there, which
    AppLocker/software-restriction script rules block on managed machines ("Dieses Programm wurde durch
    eine Gruppenrichtlinie geblockt", MSB8066 in sdl2-populate.vcxproj).
    """
    generator = os.environ.get('CMAKE_GENERATOR', '')
    if generator.startswith('Visual Studio') or (generator and generator != 'Ninja'):
        return visual_studio_platform(target), None, generator
    ninja = find_ninja(cmake)
    environment = msvc_environment(target, cmake) if ninja else None
    if ninja and environment:
        return ['-G', 'Ninja', f'-DCMAKE_MAKE_PROGRAM={ninja}'], environment, 'Ninja'
    missing = 'ninja.exe' if not ninja else f'the developer environment (vcvarsall.bat: {find_vcvarsall(cmake) or "not found"})'
    if generator == 'Ninja':
        raise RuntimeError(f'CMAKE_GENERATOR=Ninja needs ninja.exe and vcvarsall.bat; missing {missing}')
    print(f'Missing {missing}; using the Visual Studio generator. '
          'Its custom build steps run as .cmd files from the build directory, which some group policies block.', flush=True)
    return visual_studio_platform(target), None, 'Visual Studio'


def reset_foreign_generator(build: Path, generator: str) -> None:
    """A build directory configured by another generator cannot be re-configured; start it afresh."""
    cache = build / 'CMakeCache.txt'
    if not cache.is_file():
        return
    match = re.search(r'^CMAKE_GENERATOR:INTERNAL=(.*)$', cache.read_text(errors='replace'), re.M)
    if not match or match.group(1).strip().startswith(generator):
        return
    print(f'Build directory {build} was configured with "{match.group(1).strip()}"; recreating it for {generator}.', flush=True)
    shutil.rmtree(build)


IOS_TARGETS = {'ios': 'iphoneos', 'ios-simulator': 'iphonesimulator'}
IOS_DEPLOYMENT_TARGET = '15.0'  # std::filesystem ab iOS 13; Info.plist.in MinimumOSVersion gleich halten
IOS_ICONS = ('AppIcon60x60@2x.png', 'AppIcon60x60@3x.png', 'AppIcon76x76@2x~ipad.png', 'AppIcon83.5x83.5@2x~ipad.png')


def is_ios_target(target: str) -> bool:
    return target in IOS_TARGETS


def xcode_developer_dir() -> str:
    """iOS SDKs ship only with Xcode; an active CommandLineTools directory lacks them."""
    def has_ios(directory) -> bool:
        return (Path(directory) / 'Platforms/iPhoneOS.platform').is_dir()
    explicit = os.environ.get('DEVELOPER_DIR')
    if explicit:
        if not has_ios(explicit):
            raise RuntimeError(f'DEVELOPER_DIR={explicit} has no iOS platform; point it at Xcode.app/Contents/Developer')
        return explicit
    try:
        active = subprocess.check_output(['xcode-select', '-p'], text=True).strip()
    except (OSError, subprocess.CalledProcessError):
        active = ''
    if active and has_ios(active):
        return active
    for application in sorted(Path('/Applications').glob('Xcode*.app'), reverse=True):
        if has_ios(application / 'Contents/Developer'):
            return str(application / 'Contents/Developer')
    raise RuntimeError('iOS builds require Xcode with the iOS platform (xcode-select -p: '
                       f'{active or "unset"}); install Xcode or set DEVELOPER_DIR')


def ios_build_directory(target: str) -> Path:
    return ROOT / 'build' / ('ios-device' if target == 'ios' else 'ios-simulator')


def provisioning_profile(path: Path) -> dict:
    return plistlib.loads(subprocess.check_output(['security', 'cms', '-D', '-i', str(path)]))


def profile_matches_bundle(profile: dict, bundle_id: str) -> bool:
    """application-identifier is <team>.<bundle id>, optionally ending in a wildcard."""
    identifier = profile.get('Entitlements', {}).get('application-identifier', '')
    _, _, pattern = identifier.partition('.')
    return pattern == bundle_id or (pattern.endswith('*') and bundle_id.startswith(pattern[:-1]))


def signing_identity(profile: dict) -> str:
    """The keychain identity whose certificate the profile lists (SHA-1 of the DER certificate)."""
    allowed = {hashlib.sha1(certificate).hexdigest().upper() for certificate in profile.get('DeveloperCertificates', [])}
    listing = subprocess.check_output(['security', 'find-identity', '-v', '-p', 'codesigning'], text=True)
    for match in re.finditer(r'^\s*\d+\)\s+([0-9A-F]{40})\s+"([^"]+)"', listing, re.M):
        if match.group(1) in allowed:
            return match.group(1)
    raise RuntimeError(f'no keychain identity matches a certificate of the provisioning profile "{profile.get("Name", "?")}"')


def finish_ios_app(app: Path, target: str, profile_path: Path | None, bundle_data: bool) -> bool:
    """Complete Info.plist and icons, then sign. Returns False for an unsigned device build."""
    executable = app / 'recvx-runtime'
    if subprocess.check_output(['lipo', '-archs', str(executable)], text=True).split() != ['arm64']:
        raise RuntimeError('iOS runtime must be arm64-only')
    # LC_BUILD_VERSION platform 2 = iOS, 7 = iOS-Simulator; guards against a host-SDK fallback.
    load_commands = subprocess.check_output(['otool', '-l', str(executable)], text=True)
    platform_id = re.search(r'cmd LC_BUILD_VERSION\n.*?\n\s*platform (\d+)', load_commands, re.S)
    expected = '2' if target == 'ios' else '7'
    if not platform_id or platform_id.group(1) != expected:
        raise RuntimeError(f'{executable} is not built for {IOS_TARGETS[target]} (platform {platform_id.group(1) if platform_id else "?"})')
    if not bundle_data and (app / 'data').exists():
        shutil.rmtree(app / 'data')  # stale data from an earlier --ios-bundle-data build would shadow Documents/data
    info_path = app / 'Info.plist'
    with info_path.open('rb') as handle:
        info = plistlib.load(handle)
    info.update({'CFBundleVersion': FULL_VERSION, 'CFBundleShortVersionString': FULL_VERSION,
                 'CFBundleSupportedPlatforms': ['iPhoneOS' if target == 'ios' else 'iPhoneSimulator']})
    for name in IOS_ICONS:  # versioned like the ICNS/ICO, see native/resources/README.md
        shutil.copy2(ROOT / 'native/resources/ios' / name, app / name)
    primary = {'CFBundleIconFiles': ['AppIcon60x60'], 'CFBundleIconName': 'AppIcon'}
    info['CFBundleIcons'] = {'CFBundlePrimaryIcon': primary}
    info['CFBundleIcons~ipad'] = {'CFBundlePrimaryIcon': {'CFBundleIconFiles': ['AppIcon60x60', 'AppIcon76x76', 'AppIcon83.5x83.5'],
                                                          'CFBundleIconName': 'AppIcon'}}
    with info_path.open('wb') as handle:
        plistlib.dump(info, handle)
    embedded = app / 'embedded.mobileprovision'
    if target == 'ios-simulator':
        embedded.unlink(missing_ok=True)
        run('codesign', '--force', '--sign', '-', '--timestamp=none', app)
    elif profile_path is None:
        embedded.unlink(missing_ok=True)
        subprocess.run(['codesign', '--remove-signature', str(app)], cwd=ROOT, check=False)
        print('Device build is UNSIGNED and cannot be installed; pass --ios-profile <file.mobileprovision>.', flush=True)
        return False
    else:
        profile = provisioning_profile(profile_path)
        if not profile_matches_bundle(profile, info['CFBundleIdentifier']):
            raise RuntimeError(f'provisioning profile "{profile.get("Name", "?")}" does not cover {info["CFBundleIdentifier"]}; '
                               'pass the matching --ios-bundle-id')
        identity = signing_identity(profile)
        shutil.copy2(profile_path, embedded)
        with tempfile.NamedTemporaryFile('wb', suffix='.entitlements', delete=False) as handle:
            plistlib.dump(profile['Entitlements'], handle)
        try:
            run('codesign', '--force', '--sign', identity, '--entitlements', handle.name, '--generate-entitlement-der', app)
        finally:
            os.unlink(handle.name)
    run('codesign', '--verify', '--deep', '--strict', app)
    return True


def package_ios_app(app: Path, output: Path) -> None:
    """IPA = zip archive of Payload/<app>; symlinks and permissions preserved by ditto."""
    if output.exists():
        raise RuntimeError(f'package destination already exists: {output}')
    output.parent.mkdir(parents=True, exist_ok=True)
    with tempfile.TemporaryDirectory() as temporary:
        payload = Path(temporary) / 'Payload'
        payload.mkdir()
        run('ditto', app, payload / app.name)
        run('ditto', '-c', '-k', '--sequesterRsrc', '--keepParent', payload, output)
    print(f'Packaged {output}', flush=True)


def is_windows_target(target: str) -> bool:
    return target in WINDOWS_TARGETS


def cross_build_directory(target: str) -> Path:
    return ROOT / 'build' / ('windows' + ('-arm64' if WINDOWS_TARGETS[target] == 'arm64' else '-x64'))


def native_build_directory(target: str) -> Path:
    if target == 'macos':
        return ROOT / 'build/macos-arm64'
    if target == 'windows':
        return ROOT / 'build/windows-native'
    if target == 'windows-arm64':
        return ROOT / 'build/windows-arm64-native'
    return ROOT / 'build/native'


def package_directory(target: str, version: str) -> Path:
    label = {'macos': 'macos-arm64', 'windows': 'windows-x64', 'windows-arm64': 'windows-arm64', 'ios': 'ios-arm64'}[target]
    return ROOT / 'build/packages' / f'{label}-v{version}'


def host_tools(target: str, binary_dir: Path, names) -> list[Path]:
    """Build-host executables (exporters, compressor, mod baker). Cross builds (Windows from macOS/Linux,
    iOS) run the tools of the native host build, which ./build.sh --platform macos creates."""
    if (is_windows_target(target) and platform.system() != 'Windows') or is_ios_target(target):
        directories, suffix = [ROOT / 'build/macos-arm64', ROOT / 'build/native'], ''
    else:
        directories, suffix = [binary_dir], '.exe' if is_windows_target(target) else ''
    names = [name + suffix for name in names]
    directory = next((d for d in directories if all((d / n).is_file() for n in names)), None)
    if directory is None:
        raise RuntimeError(f'missing build-host tools {" and ".join(names)} in {", ".join(map(str, directories))}; '
                           'build the host first (./build.sh --platform macos)')
    return [directory / name for name in names]


def prepare_iso_data(images, data: Path, binary_dir: Path, target: str, jobs: int) -> str:
    """Create the complete loose data tree from ISO images only when none exists yet."""
    if (data / 'native-data.version').is_file():
        print(f'Using existing prepared data at {data}; --iso is not extracted again.', flush=True)
        return f'{data}: existing tree kept (--iso extracts only into a missing folder)'
    if data.exists():
        raise RuntimeError(f'--iso needs a new data directory, but {data} exists without a completed export')
    exporter, _ = host_tools(target, binary_dir, ('recvx-assets', 'recvx-world-assets'))
    command = [sys.executable, str(ROOT / 'tools/prepare_native_data.py'),
               '--output', str(data), '--exporter', str(exporter), '--jobs', str(jobs)]
    for image in images:
        command += ['--iso', str(image.absolute())]  # run() starts in ROOT, not in the caller's directory
    run(*command)
    return f'{data}: extracted from {", ".join(str(i) for i in images)}'


def prepare_compressed_data(data: Path, output: Path, binary_dir: Path, target: str, jobs: int) -> str:
    """Create the lossless compressed tree (colour-mapped TGA, recooked movies) from a prepared --data."""
    if (output / 'native-data.version').is_file():
        print(f'Using existing compressed data at {output}; --compressed is not written again.', flush=True)
        return f'{output}: existing tree kept (delete it to rebuild it from {data})'
    if output.exists():
        raise RuntimeError(f'--compressed needs a new directory, but {output} exists without a completed tree')
    if not (data / 'native-data.version').is_file():
        raise RuntimeError(f'--compressed derives from a prepared data tree (missing {data / "native-data.version"})')
    exporter, _ = host_tools(target, binary_dir, ('recvx-assets', 'recvx-texture-compress'))
    run(sys.executable, str(ROOT / 'tools/prepare_native_data.py'), '--compressed', '--from-data', str(data),
        '--output', str(output), '--exporter', str(exporter), '--jobs', str(jobs))
    return f'{output}: written from {data}'


def import_hd_sources(mods: Path, data: Path, jobs: int) -> str:
    """mods/hd-source/**.dds -> mods/textures/hd/<md5>.tga (tools/import_hd_textures.py; skipped when unchanged)."""
    source = mods / 'hd-source'
    stray = sum(1 for p in (mods / 'textures').rglob('*') if p.suffix.lower() == '.dds') if (mods / 'textures').is_dir() else 0
    note = (f'; WARNING: {stray} DDS in {mods / "textures"} are never read, move the pack to {source}' if stray else '')
    if not source.is_dir():
        return f'no {source} folder (DDS packs go there){note}'
    if not (data / 'native-data.version').is_file():
        return f'skipped, {source} needs the uncompressed tree {data}{note}'
    try:
        import import_hd_textures
    except ImportError as error:
        raise RuntimeError(f'HD import of {source} needs Pillow ({error}); install it: python3 -m pip install Pillow')
    lines = []
    import_hd_textures.run(mods, data, 4, 'ABL', jobs, log=lambda line: (print(line, flush=True), lines.append(line)))
    return (lines[-1] if lines else f'nothing to import from {source}') + note


def bundle_tree(target: str, data: Path, compressed: Path | None, binary_dir: Path, jobs: int) -> tuple[Path, bool]:
    """The tree an app bundles: data_compressed with --compressed or when the platform always needs it (tvOS)."""
    if compressed is None and data_profile(target)['compressed'] != 'always':
        return data, False
    tree = (compressed or ROOT / 'data_compressed').resolve()
    prepare_compressed_data(data, tree, binary_dir, target, jobs)
    return tree, True


def stage_app_data(target: str, source: Path, compressed: bool, mods: Path, binary_dir: Path) -> tuple[Path, dict]:
    """Private copy of the bundle tree with mods baked in; the app gets no mods folder."""
    bake = None
    if mods_with_content(mods) is not None:
        bake = host_tools(target, binary_dir, ('recvx-mod-bake',))[0]
    staging = ROOT / 'build/bundle-data' / target
    print(f'Staging app data from {source} in {staging} (mods from {mods} baked in) ...', flush=True)
    return staging, stage_bundle_data(source, staging, target, mods, bake, compressed)


def print_summary(lines) -> None:
    print('\n== RECVX build summary ==', flush=True)
    for label, text in lines:
        print(f'{label + ":":<14}{text}', flush=True)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--platform', choices=['host', 'macos', 'linux', 'windows', 'windows-arm64', 'ios', 'ios-simulator'], default='host')
    parser.add_argument('--jobs', type=int, default=min(os.cpu_count() or 4, 8))
    parser.add_argument('--data', type=Path, default=ROOT / 'data')
    parser.add_argument('--iso', type=Path, action='append',
                        help='PS2 ISO9660 image used to create missing --data; repeat to merge discs')
    parser.add_argument('--mods', type=Path, default=ROOT / 'mods',
                        help='mods folder: hd-source/ is imported into textures/hd, packages bake it into their data '
                             '(default: mods/ in the project)')
    parser.add_argument('--compressed', nargs='?', type=Path, const=ROOT / 'data_compressed',
                        help='Also write the lossless compressed tree from --data (after --iso), default data_compressed; '
                             'an existing completed tree is kept')
    parser.add_argument('--package', action='store_true')
    parser.add_argument('--output', type=Path, help='New package directory; refuses overwrites')
    parser.add_argument('--verify-data', action='store_true')
    parser.add_argument('--test', action='store_true', help='Build test targets and run CTest')
    parser.add_argument('--test-regex', help='Build test targets and run only matching CTest names')
    parser.add_argument('--opengl', choices=['on', 'off'], default='on', help='Build optional OpenGL backend (default: on)')
    parser.add_argument('--vulkan', choices=['on', 'off'], default='on', help='Build optional Vulkan backend when Vulkan headers and glslc are present (default: on)')
    parser.add_argument('--verify-runtime', action='store_true', help='Run the opt-in original-data runtime regression suite')
    parser.add_argument('--no-test', action='store_true', help=argparse.SUPPRESS)
    parser.add_argument('--ios-profile', type=Path, help='iOS device: provisioning profile used for signing (else unsigned)')
    parser.add_argument('--ios-bundle-id', default='org.recvx.native', help='iOS bundle identifier (default: org.recvx.native)')
    parser.add_argument('--ios-bundle-data', action='store_true', help='iOS: copy --data into the app bundle (else the app reads Documents/data)')
    parser.add_argument('--install', action='store_true', help='iOS: install the app on the booted simulator or on --ios-device')
    parser.add_argument('--ios-device', help='iOS device identifier or name for --install (xcrun devicectl list devices)')
    args = parser.parse_args()
    run(sys.executable, ROOT / 'tools/check_no_original_core.py', ROOT)
    if not 1 <= args.jobs <= 256:
        parser.error('--jobs must be between 1 and 256')
    target = args.platform
    if target == 'host':
        target = host_target(platform.system(), host_machine())
    if not target:
        parser.error('unsupported host')
    data = args.data.resolve()
    mods = args.mods.resolve()
    summary = []
    if is_ios_target(target):
        if platform.system() != 'Darwin':
            parser.error('iOS builds require a macOS host with Xcode')
        if args.test or args.test_regex or args.verify_runtime or args.iso or args.verify_data:
            parser.error('iOS builds cannot run tests or prepare data; build and verify them with the macOS target')
        if args.ios_bundle_data and not (data / 'native-data.version').is_file():
            parser.error(f'--ios-bundle-data requires a prepared data tree (missing {data / "native-data.version"})')
        if args.package and target != 'ios':
            parser.error('--package for iOS creates a device IPA; use --platform ios')
        if args.install and target == 'ios' and not args.ios_device:
            parser.error('--install on a device needs --ios-device')
    elif args.ios_profile or args.ios_bundle_data or args.install or args.ios_device:
        parser.error('--ios-* and --install options apply only to --platform ios or ios-simulator')
    if is_windows_target(target) and platform.system() != 'Windows' and (args.test or args.test_regex or args.verify_runtime):
        parser.error('Windows cross-builds cannot run tests; use --platform windows or windows-arm64 without test options, then test on Windows or Wine')
    cmake = find_cmake()
    if cmake is None:
        parser.error('CMake was not found on PATH or in a Visual Studio installation. Install CMake (or in the Visual Studio '
                     'Installer the CMake tools component; on some installs it is only listed under the Linux/Mac '
                     'development workload) and open a new terminal; verify with "cmake --version" before building.')
    print(f'Target {target}, CMake {cmake}', flush=True)
    summary.append(('Target', target))
    run_checks = (args.test or bool(args.test_regex)) and not args.no_test
    if is_windows_target(target) and platform.system() != 'Windows':
        # The final binary audit also invokes Zig; use the same project cache.
        os.environ['ZIG_GLOBAL_CACHE_DIR'] = str(ROOT / 'build/toolchains/zig-cache')
        run(sys.executable, ROOT / 'tools/build_windows.py', '--arch', WINDOWS_TARGETS[target], '--jobs', args.jobs, '--vulkan', args.vulkan, '--opengl', args.opengl)
        build = cross_build_directory(target)
        print(f'Cross-built Windows {WINDOWS_TARGETS[target]}.', flush=True)
    elif is_ios_target(target):
        if os.environ.get('CMAKE_GENERATOR', '').startswith('Xcode'):
            parser.error('iOS uses a single-config generator: the policy audit needs compile_commands.json, which Xcode does not write')
        os.environ['DEVELOPER_DIR'] = xcode_developer_dir()
        build = ios_build_directory(target)
        bundle = ''
        if args.ios_bundle_data:
            summary.append(('HD import', import_hd_sources(mods, data, args.jobs)))
            tree, compressed = bundle_tree(target, data, args.compressed, build, args.jobs)
            staging, staged = stage_app_data(target, tree, compressed, mods, build)
            bundle = str(staging)
            summary.append(('App data', f'{tree} ({"compressed" if compressed else "uncompressed"}) copied into the app; '
                                        f'mods: {staged["bake"]}; no runtime mod support on {target}'))
        else:
            summary.append(('App data', 'not bundled: the app reads Documents/data (pass --ios-bundle-data to bundle it)'))
        run(cmake, '-S', ROOT, '-B', build, '-DCMAKE_BUILD_TYPE=Release', '-DCMAKE_SYSTEM_NAME=iOS',
            f'-DCMAKE_OSX_SYSROOT={IOS_TARGETS[target]}', '-DCMAKE_OSX_ARCHITECTURES=arm64',
            f'-DCMAKE_OSX_DEPLOYMENT_TARGET={IOS_DEPLOYMENT_TARGET}', '-DRECVX_FETCH_SDL2=ON', '-DBUILD_TESTING=OFF',
            f'-DRECVX_BUILD_VERSION={FULL_VERSION}', '-DRECVX_ENABLE_OPENGL=OFF', f'-DRECVX_IOS_BUNDLE_ID={args.ios_bundle_id}',
            f'-DRECVX_IOS_BUNDLE_DATA={bundle}',
            f'-DRECVX_BUNDLE_DATA_EXCLUDE={";".join(data_profile(target)["exclude"])}',
            f'-DRECVX_ENABLE_MODS={"ON" if data_profile(target)["mods"] else "OFF"}')
        run(cmake, '--build', build, '--config', 'Release', '--parallel', args.jobs)
        app = build / 'recvx-runtime.app'
        run(sys.executable, ROOT / 'tools/check_no_original_core.py', ROOT, '--build-dir', build, '--binary', app / 'recvx-runtime')
        signed = finish_ios_app(app, target, args.ios_profile.resolve() if args.ios_profile else None, args.ios_bundle_data)
        if args.package:
            if not signed:
                parser.error('--package needs a signed device build; pass --ios-profile')
            package_ios_app(app, (args.output or package_directory(target, VERSION)) / 'RECVX-Native.ipa')
        if args.install:
            if target == 'ios-simulator':
                run('xcrun', 'simctl', 'install', 'booted', app)
                print(f'Installed; start with: xcrun simctl launch booted {args.ios_bundle_id}', flush=True)
            elif not signed:
                parser.error('--install on a device needs a signed build; pass --ios-profile')
            else:
                run('xcrun', 'devicectl', 'device', 'install', 'app', '--device', args.ios_device, app)
        summary.append(('App', str(app)))
        print_summary(summary)
        return
    else:
        if target == 'macos' and platform.system() != 'Darwin':
            parser.error('macOS builds require a macOS host')
        if target == 'linux' and platform.system() != 'Linux':
            parser.error('Linux builds require a Linux host')
        build = native_build_directory(target)
        configure = [cmake, '-S', ROOT, '-B', build, '-DCMAKE_BUILD_TYPE=Release', '-DRECVX_FETCH_SDL2=ON', f'-DBUILD_TESTING={"ON" if run_checks or args.verify_runtime else "OFF"}', f'-DRECVX_BUILD_VERSION={FULL_VERSION}', f'-DRECVX_ENABLE_OPENGL={args.opengl.upper()}', f'-DRECVX_ENABLE_VULKAN={args.vulkan.upper()}']
        if target == 'macos':
            configure += ['-DCMAKE_OSX_ARCHITECTURES=arm64', '-DCMAKE_OSX_DEPLOYMENT_TARGET=11.0']
        if is_windows_target(target):
            arguments, environment, generator = windows_generator(target, cmake)
            configure += arguments
            if environment:
                os.environ.update(environment)
            reset_foreign_generator(build, generator)
        run(*configure)
        run(cmake, '--build', build, '--config', 'Release', '--parallel', args.jobs)
    binary_dir = build / 'Release' if (build / 'Release').is_dir() else build
    suffix = '.exe' if is_windows_target(target) else ''
    if args.iso:
        summary.append(('ISO export', prepare_iso_data(args.iso, data, binary_dir, target, args.jobs)))
    summary.append(('Data', f'{data}' if (data / 'native-data.version').is_file() else f'{data} MISSING (build with --iso <image>)'))
    summary.append(('HD import', import_hd_sources(mods, data, args.jobs)))
    if args.compressed:
        summary.append(('Compressed', prepare_compressed_data(data, args.compressed.resolve(), binary_dir, target, args.jobs)))
    if args.verify_runtime and not (data / 'native-data.version').is_file():
        parser.error(f'--verify-runtime requires a prepared native data tree (missing {data / "native-data.version"})')
    if run_checks:
        test_command = [str(Path(cmake).with_name('ctest' + Path(cmake).suffix)), '--test-dir', build, '-C', 'Release', '--output-on-failure']
        if args.test_regex:
            test_command += ['-R', args.test_regex, '--no-tests=error']
        run(*test_command)
    run(sys.executable, ROOT / 'tools/check_no_original_core.py', ROOT,
        '--build-dir', build, '--binary', binary_dir / ('recvx-runtime' + suffix))
    if args.verify_data:
        run(sys.executable, ROOT / 'tools/prepare_native_data.py', '--output', data, '--verify')
    if not is_windows_target(target) or platform.system() == 'Windows':
        suffix = '.exe' if is_windows_target(target) else ''
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
            parser.error('the packaged targets are Windows (x64/ARM64) and macOS')
        output = (args.output or package_directory(target, VERSION)).resolve()
        if output.exists():
            parser.error(f'package destination already exists: {output}')
        tree, compressed = bundle_tree(target, data, args.compressed, binary_dir, args.jobs)
        staging, staged = stage_app_data(target, tree, compressed, mods, binary_dir)
        run(sys.executable, ROOT / 'tools/package_native.py', '--platform', target, '--build', binary_dir, '--data', staging, '--output', output)
        shutil.rmtree(staging)
        summary.append(('Package', f'{output}'))
        summary.append(('Package data', f'{tree} ({"compressed" if compressed else "uncompressed"}) copied into the package; '
                                        f'mods: {staged["bake"]}; no mods folder shipped'))
    summary.append(('Runtime', str(binary_dir / ('recvx-runtime' + suffix))))
    if target == 'macos':
        sync_macos_start_package(binary_dir)
        summary.append(('Dev app', f'{ROOT / "build/start/macos-arm64/RECVX Native.app"} (Start.command) reads LOOSE files: '
                                   f'--data {data} --mods {mods}; nothing is copied, data/mod changes need no rebuild'))
    print_summary(summary)


if __name__ == '__main__':
    main()
