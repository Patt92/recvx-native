"""tools/web_site.py: data index for the browser build (native/web_main.cpp reads it to build /data)."""
import sys
import tempfile
import unittest
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parents[1] / 'tools'))
import web_site  # noqa: E402


class WebSiteTests(unittest.TestCase):
    def tree(self, root: Path, files):
        for name in files:
            path = root / name
            path.parent.mkdir(parents=True, exist_ok=True)
            path.write_bytes(b'x')

    def test_listing_is_sorted_posix_and_skips_hidden_index_and_excluded(self):
        with tempfile.TemporaryDirectory() as folder:
            root = Path(folder)
            self.tree(root, ['rooms/RM_0010.RDT', 'rooms/RM_0000.RDT', '.DS_Store', 'textures/.DS_Store',
                             'files/MOVIE/MV_000.PSS', 'files/SYSMES.ALD', 'archives/RDX_LNK/0000.bin',
                             'archives/ITEM1/0000.bin', web_site.LISTING, 'native-data.version'])
            listing = web_site.tree_listing(root, ('files/MOVIE', 'archives/RDX_LNK'))
            self.assertEqual(listing, ['archives/ITEM1/0000.bin', 'files/SYSMES.ALD', 'native-data.version',
                                       'rooms/RM_0000.RDT', 'rooms/RM_0010.RDT'])

    def test_index_is_written_once_and_only_rewritten_on_change(self):
        with tempfile.TemporaryDirectory() as folder:
            root = Path(folder)
            self.tree(root, ['a/b.bin'])
            self.assertEqual(web_site.write_listing(root, web_site.LISTING), 1)
            stamp = (root / web_site.LISTING).stat().st_mtime_ns
            self.assertEqual(web_site.write_listing(root, web_site.LISTING), 1)
            self.assertEqual((root / web_site.LISTING).stat().st_mtime_ns, stamp)
            self.assertEqual((root / web_site.LISTING).read_text(), 'a/b.bin\n')

    def test_site_requires_the_build_output(self):
        with tempfile.TemporaryDirectory() as folder:
            with self.assertRaisesRegex(RuntimeError, 'web build output missing'):
                web_site.assemble_site(Path(folder), Path(folder) / 'site')


if __name__ == '__main__':
    unittest.main()
