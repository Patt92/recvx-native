import sys,tempfile,unittest
from pathlib import Path
sys.path.insert(0,str(Path(__file__).resolve().parents[1]/'tools'))
import data_profiles

class DataProfileTests(unittest.TestCase):
    def test_targets_map_to_profiles(self):
        self.assertEqual(data_profiles.profile('macos')['exclude'],())
        self.assertIn('files/MOVIE',data_profiles.profile('ios-simulator')['exclude'])
        self.assertTrue(data_profiles.profile('macos')['mods'])
        self.assertFalse(data_profiles.profile('ios')['mods'])
        self.assertEqual(data_profiles.profile('tvos')['compressed'],'always')
        self.assertEqual(data_profiles.profile('windows')['compressed'],'optional')
        self.assertFalse(data_profiles.profile('tvos')['mods'])
        self.assertIn('archives/RDX_LNK',data_profiles.profile('android')['exclude'])
        self.assertTrue(data_profiles.profile('android')['mods'])
        self.assertIn('files/MOVIE',data_profiles.profile('web')['exclude'])
        self.assertTrue(data_profiles.profile('web')['mods'])
    def test_copy_leaves_out_only_profile_subtrees(self):
        with tempfile.TemporaryDirectory() as t:
            s=Path(t)/'s'
            for f in ('files/MOVIE/MV_000.PSS','files/SYSMES.ALD','archives/RDX_LNK/0000.bin','archives/ITEM1/0000.bin','movies/0000.rvv'):
                (s/f).parent.mkdir(parents=True,exist_ok=True);(s/f).write_text('x')
            data_profiles.copy_data(s,Path(t)/'i','ios');data_profiles.copy_data(s,Path(t)/'d','macos')
            self.assertFalse((Path(t)/'i/files/MOVIE').exists());self.assertFalse((Path(t)/'i/archives/RDX_LNK').exists())
            self.assertTrue((Path(t)/'i/files/SYSMES.ALD').is_file());self.assertTrue((Path(t)/'i/archives/ITEM1/0000.bin').is_file())
            self.assertTrue((Path(t)/'d/files/MOVIE/MV_000.PSS').is_file())
if __name__=='__main__':unittest.main()
