#!/usr/bin/env python3
"""Structural check of a built iOS/iPadOS or tvOS app bundle (and optionally its IPA).

Checks what an in-place IPA update and the app start depend on: bundle id, a CFBundleVersion that matches
tools/build_native.py FULL_VERSION, platform and architecture of the executable, the UIScene delegate
(iOS 27 SDK), file sharing for Documents/data, ProMotion, iPhone+iPad device families, the touch gamepad and its
orientations, icons, weakly linked
MetalFX and no stale data/ in a slim bundle (--ios-no-bundle-data). It reads only; Xcode command-line
tools (otool, nm, lipo) are required. Exit 0 = ok, 1 = finding, 77 = no bundle to check (CTest skip)."""
import argparse
import importlib.util
from pathlib import Path
import plistlib
import re
import stat
import subprocess
import sys
import zipfile

ROOT = Path(__file__).resolve().parents[1]
spec = importlib.util.spec_from_file_location('build_native', ROOT / 'tools/build_native.py')
build_native = importlib.util.module_from_spec(spec)
spec.loader.exec_module(build_native)


def tool(*command):
    return subprocess.run(command, check=True, capture_output=True, text=True).stdout


def check_app(app: Path, target: str, bundle_data: bool | None, bundle_id: str) -> list[str]:
    findings = []
    expect = lambda ok, text: ok or findings.append(text)
    with (app / 'Info.plist').open('rb') as handle:
        info = plistlib.load(handle)
    ios = target.startswith('ios')
    expect(info.get('CFBundleIdentifier') == bundle_id, f'CFBundleIdentifier {info.get("CFBundleIdentifier")!r}, expected {bundle_id!r}')
    expect(info.get('CFBundleVersion') == build_native.FULL_VERSION,
           f'CFBundleVersion {info.get("CFBundleVersion")!r} is not this checkout\'s {build_native.FULL_VERSION!r} (stale build?)')
    expect(info.get('CFBundleShortVersionString') == info.get('CFBundleVersion'), 'CFBundleShortVersionString differs from CFBundleVersion')
    expect(re.fullmatch(r'\d+\.\d+\.[1-9]\d*', str(info.get('CFBundleVersion', ''))) is not None,
           'CFBundleVersion has build number 0: built without git history, an update would not be newer')
    expect(info.get('CFBundleSupportedPlatforms') == [build_native.APPLE_TARGETS[target][2]], f'CFBundleSupportedPlatforms {info.get("CFBundleSupportedPlatforms")}')
    expect(info.get('MinimumOSVersion') == build_native.IOS_DEPLOYMENT_TARGET, f'MinimumOSVersion {info.get("MinimumOSVersion")}')
    scenes = info.get('UIApplicationSceneManifest', {}).get('UISceneConfigurations', {}).get('UIWindowSceneSessionRoleApplication', [])
    expect(any(s.get('UISceneDelegateClassName') == 'RECVXSceneDelegate' for s in scenes), 'no UIScene manifest with RECVXSceneDelegate')
    if ios:
        expect(set(info.get('UIDeviceFamily', [])) == {1, 2}, f'UIDeviceFamily {info.get("UIDeviceFamily")}, expected iPhone and iPad')
        # Touch gamepad (native/ios_touch_pad.mm): landscape overlay and the portrait split need all three orientations.
        wanted = {'UIInterfaceOrientationLandscapeLeft', 'UIInterfaceOrientationLandscapeRight', 'UIInterfaceOrientationPortrait'}
        for key in ('UISupportedInterfaceOrientations', 'UISupportedInterfaceOrientations~ipad'):
            expect(wanted <= set(info.get(key, [])), f'{key} {info.get(key)} lacks landscape or portrait (touch gamepad layouts)')
        expect(info.get('UIFileSharingEnabled') is True and info.get('LSSupportsOpeningDocumentsInPlace') is True,
               'file sharing for Documents/data is off')
        expect(info.get('CADisableMinimumFrameDurationOnPhone') is True, 'CADisableMinimumFrameDurationOnPhone missing (ProMotion)')
    for name in build_native.IOS_ICONS if ios else build_native.TVOS_ICONS:
        expect((app / name).is_file(), f'icon {name} missing')
    executable = app / info.get('CFBundleExecutable', 'recvx-runtime')
    expect(tool('lipo', '-archs', str(executable)).split() == ['arm64'], 'executable is not arm64-only')
    platform = re.search(r'cmd LC_BUILD_VERSION\n.*?\n\s*platform (\d+)', tool('otool', '-l', str(executable)), re.S)
    expect(platform is not None and platform.group(1) == build_native.APPLE_TARGETS[target][1],
           f'LC_BUILD_VERSION platform {platform.group(1) if platform else "?"}, expected {build_native.APPLE_TARGETS[target][1]}')
    symbols = tool('nm', '-U', str(executable))
    expect('RECVXSceneDelegate' in symbols, 'RECVXSceneDelegate class not in the executable')
    expect(('RECVXTouchPad' in symbols) == ios, 'touch gamepad (RECVXTouchPad) ' + ('missing from the iOS executable' if ios else 'present in a tvOS executable'))
    metalfx = [line for line in tool('otool', '-L', str(executable)).splitlines() if 'MetalFX.framework' in line]
    if target.endswith('simulator'):
        expect(not metalfx, 'simulator app links MetalFX (the simulator SDKs have none)')
    else:
        expect(bool(metalfx) and all('weak' in line for line in metalfx), 'MetalFX is not weakly linked (app would not start below iOS 16)')
    has_data = (app / 'data/native-data.version').is_file()
    if bundle_data is None:  # nicht vorgegeben: Variante aus dem Bundle, ein data/ ohne Versionsdatei bleibt ein Fund
        bundle_data = (app / 'data').exists()
    if bundle_data:
        expect(has_data, 'bundled data/ expected (default build) but missing')
    else:
        expect(not (app / 'data').exists(), 'data/ inside the bundle shadows Documents/data and is re-sent on every update')
    return findings


def check_ipa(ipa: Path, app_name: str) -> list[str]:
    findings = []
    with zipfile.ZipFile(ipa) as archive:
        names = archive.namelist()
        if not names or not all(n.startswith(f'Payload/{app_name}') for n in names):
            findings.append('IPA entries outside Payload/<app>')
        if any('__MACOSX' in n for n in names):
            findings.append('IPA carries __MACOSX entries')
        executable = archive.getinfo(f'Payload/{app_name}/recvx-runtime') if f'Payload/{app_name}/recvx-runtime' in names else None
        if executable is None or not stat.S_IMODE(executable.external_attr >> 16) & 0o100:
            findings.append('IPA executable missing or not executable')
        if archive.testzip() is not None:
            findings.append('IPA has a corrupt entry')
    return findings


def main():
    p = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    p.add_argument('--target', choices=sorted(build_native.IOS_TARGETS), default='ios')
    p.add_argument('--app', type=Path, help='app bundle (default: build/<target dir>/recvx-runtime.app)')
    p.add_argument('--ipa', type=Path, help='also check this IPA')
    variant = p.add_mutually_exclusive_group()
    variant.add_argument('--bundle-data', dest='bundle_data', action='store_true', default=None,
                         help='expect the game data in the bundle (default build)')
    variant.add_argument('--slim', dest='bundle_data', action='store_false',
                         help='expect no data/ in the bundle (--ios-no-bundle-data); without either option the bundle decides')
    p.add_argument('--bundle-id', default='org.recvx.native')
    a = p.parse_args()
    app = a.app or build_native.ios_build_directory(a.target) / 'recvx-runtime.app'
    if not (app / 'Info.plist').is_file():
        print(f'no app bundle at {app}; nothing to check')
        return 77
    findings = check_app(app, a.target, a.bundle_data, a.bundle_id)
    if a.ipa:
        findings += check_ipa(a.ipa, app.name)
    for finding in findings:
        print(f'FAIL: {finding}', file=sys.stderr)
    if not findings:
        print(f'{app.name} ({a.target}, {build_native.FULL_VERSION}) ok' + (f', {a.ipa.name} ok' if a.ipa else ''))
    return 1 if findings else 0


if __name__ == '__main__':
    sys.exit(main())
