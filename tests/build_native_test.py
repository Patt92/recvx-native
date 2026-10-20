"""Development-app publishing; macOS tools are mocked, not policy bypassed."""
import importlib.util
from pathlib import Path
import plistlib
import subprocess
import tempfile
import unittest
from unittest.mock import patch

spec = importlib.util.spec_from_file_location('build_native', Path(__file__).resolve().parents[1] / 'tools/build_native.py')
build_native = importlib.util.module_from_spec(spec)
spec.loader.exec_module(build_native)


class DevelopmentAppTests(unittest.TestCase):
    def setUp(self):
        self.temporary = tempfile.TemporaryDirectory(prefix='recvx build test ')
        self.addCleanup(self.temporary.cleanup)
        self.root = Path(self.temporary.name)
        self.build = self.root / 'build/macos-arm64'
        self.library = self.build / '_deps/sdl2-build/libSDL2-2.0.dylib'
        self.library.parent.mkdir(parents=True)
        self.library.write_bytes(b'fixture SDL')
        (self.build / 'recvx-runtime').write_bytes(b'fixture native runtime')
        (self.build / 'recvx-runtime').chmod(0o755)
        (self.root / 'controls.ini').write_text('user configuration')
        self.icon = self.root / 'native/resources/app-icon.icns'
        self.icon.parent.mkdir(parents=True)
        self.icon.write_bytes(b'fixture icon')
        self.app = self.root / 'build/start/macos-arm64/RECVX Native.app'
        self.commands = []
        self.addCleanup(patch.stopall)
        patch.object(build_native, 'ROOT', self.root).start()
        patch.object(build_native.subprocess, 'check_output', side_effect=self.inspect).start()
        patch.object(build_native, 'run', side_effect=self.record_command).start()

    def inspect(self, command, **kwargs):
        if command[0] == 'lipo':
            return 'arm64\n'
        return 'fixture:\n\t@rpath/libSDL2-2.0.dylib (compatibility version 1.0.0)\n'

    def record_command(self, *command):
        self.commands.append(tuple(map(str, command)))
        if command[0] == 'codesign' and Path(command[-1]).suffix == '.app':
            contents = Path(command[-1]) / 'Contents'
            with (contents / 'Info.plist').open('rb') as info:
                self.assertEqual(plistlib.load(info)['CFBundleIconFile'], 'app-icon.icns')
            self.assertEqual((contents / 'Resources/app-icon.icns').read_bytes(), self.icon.read_bytes())

    def test_creates_without_any_old_package(self):
        build_native.sync_macos_start_package(self.build)
        self.assertEqual((self.app / 'Contents/MacOS/recvx-runtime').read_bytes(), b'fixture native runtime')
        self.assertEqual((self.app / 'Contents/Frameworks/libSDL2-2.0.dylib').read_bytes(), b'fixture SDL')
        launcher = self.app / 'Contents/MacOS/Start'
        self.assertTrue(launcher.stat().st_mode & 0o111)
        self.assertIn(str(self.root / 'controls.ini'), launcher.read_text())
        self.assertIn('--debug-config', launcher.read_text())
        self.assertIn('"$@"', launcher.read_text())
        self.assertTrue(any('check_no_original_core.py' in ' '.join(c) and '--binary' in c for c in self.commands))
        self.assertTrue(any(c[:4] == ('codesign', '--verify', '--deep', '--strict') for c in self.commands))

    def test_retired_package_is_not_consumed_or_restored(self):
        retired = self.root / 'build/packages/macos-arm64-v0.5/RECVX Native.app/Contents/MacOS'
        retired.mkdir(parents=True)
        build_native.sync_macos_start_package(self.build)
        self.assertEqual(list(retired.iterdir()), [])
        self.assertTrue((self.app / 'Contents/MacOS/Start').is_file())

    def test_refresh_retains_previous_app_and_user_settings(self):
        build_native.sync_macos_start_package(self.build)
        (self.build / 'recvx-runtime').write_bytes(b'new native runtime')
        build_native.sync_macos_start_package(self.build)
        self.assertEqual((self.app / 'Contents/MacOS/recvx-runtime').read_bytes(), b'new native runtime')
        backups = list(self.app.parent.glob('.previous-*/RECVX Native.app/Contents/MacOS/recvx-runtime'))
        self.assertEqual(len(backups), 1)
        self.assertEqual(backups[0].read_bytes(), b'fixture native runtime')
        self.assertEqual((self.root / 'controls.ini').read_text(), 'user configuration')

    def test_failed_audit_does_not_publish_over_previous_app(self):
        build_native.sync_macos_start_package(self.build)
        def fail_audit(*command):
            if '--binary' in command:
                raise subprocess.CalledProcessError(1, command)
        with patch.object(build_native, 'run', side_effect=fail_audit):
            with self.assertRaises(subprocess.CalledProcessError):
                build_native.sync_macos_start_package(self.build)
        self.assertEqual((self.app / 'Contents/MacOS/recvx-runtime').read_bytes(), b'fixture native runtime')
        self.assertEqual(list(self.app.parent.glob('.staging-*')), [])

    def test_missing_icon_does_not_publish_over_previous_app(self):
        build_native.sync_macos_start_package(self.build)
        self.icon.unlink()
        with self.assertRaises(FileNotFoundError):
            build_native.sync_macos_start_package(self.build)
        self.assertEqual((self.app / 'Contents/Resources/app-icon.icns').read_bytes(), b'fixture icon')
        self.assertEqual(list(self.app.parent.glob('.previous-*')), [])
        self.assertEqual(list(self.app.parent.glob('.staging-*')), [])

    def test_missing_sdl_fails_before_creating_app(self):
        self.library.unlink()
        with self.assertRaisesRegex(RuntimeError, 'built SDL2 library'):
            build_native.sync_macos_start_package(self.build)
        self.assertFalse(self.app.exists())


class IsoPreparationTests(unittest.TestCase):
    def setUp(self):
        self.temporary = tempfile.TemporaryDirectory(prefix='recvx iso build test ')
        self.addCleanup(self.temporary.cleanup)
        self.root = Path(self.temporary.name)
        self.data = self.root / 'data'
        self.binary = self.root / 'build/macos-arm64'
        self.exporter = self.binary / 'recvx-assets'
        self.exporter.parent.mkdir(parents=True)
        self.exporter.write_bytes(b'fixture exporter')
        self.commands = []
        self.addCleanup(patch.stopall)
        patch.object(build_native, 'ROOT', self.root).start()
        patch.object(build_native, 'run', side_effect=lambda *command: self.commands.append(tuple(map(str, command)))).start()

    def test_missing_data_is_extracted_from_iso(self):
        build_native.prepare_iso_data([self.root / 'game.iso'], self.data, self.binary, 'macos')
        self.assertEqual(len(self.commands), 1)
        command = self.commands[0]
        self.assertIn(str(self.root / 'tools/prepare_native_data.py'), command)
        self.assertIn('--iso', command)
        self.assertIn(str(self.root / 'game.iso'), command)
        self.assertIn(str(self.exporter), command)

    def test_existing_data_is_not_replaced(self):
        self.data.mkdir()
        (self.data / 'native-data.version').write_text('recvx-assets-1\n')
        build_native.prepare_iso_data([self.root / 'game.iso'], self.data, self.binary, 'macos')
        self.assertEqual(self.commands, [])

    def test_missing_exporter_fails(self):
        self.exporter.unlink()
        with self.assertRaisesRegex(RuntimeError, 'built exporter'):
            build_native.prepare_iso_data([self.root / 'game.iso'], self.data, self.binary, 'macos')


if __name__ == '__main__':
    unittest.main()
