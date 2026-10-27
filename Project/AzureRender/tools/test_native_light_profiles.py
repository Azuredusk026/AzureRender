import copy
import json
from pathlib import Path
import struct
import subprocess
import sys
import tempfile
import unittest

from endfield_native.light_profiles import bind_specular_profiles


class NativeLightProfileTests(unittest.TestCase):
    def setUp(self):
        self.level={'schemaVersion':1,'lights':[
            {'id':'native-light-7','shape':{'type':'point','specularScale':1},'intensity':3},
            {'id':'native-light-9','shape':{'type':'spot'}},
            {'id':'project-sun','shape':{'type':'directional','specularScale':.25}}]}
        self.profile={'format':'AzureEndfieldEcsLights/1','scene':'fixture','rows':[
            {'entity_id':7,'identity':'source-seven','fields':{'specular_intensity':.579}},
            {'entity_id':9,'identity':'source-nine','fields':{'specular_intensity':0}}]}

    def bind(self):return bind_specular_profiles(self.level,self.profile,source_scene='fixture')

    def test_binding_uses_source_identity_and_preserves_other_fields(self):
        before=copy.deepcopy(self.level);original=copy.deepcopy(self.profile)
        result,receipt=self.bind()
        self.assertEqual(result['lights'][0]['shape']['specularScale'],struct.unpack('<f',struct.pack('<f',.579))[0])
        self.assertEqual(result['lights'][1]['shape']['specularScale'],0)
        self.assertEqual(result['lights'][2],before['lights'][2]);self.assertEqual(result['lights'][0]['intensity'],3)
        self.assertEqual(self.level,before);self.assertEqual(self.profile,original)
        self.assertEqual(receipt['boundLights'],2);self.assertEqual(receipt['unmappedLights'],['project-sun'])

    def test_source_scene_and_format_are_explicit(self):
        for key,value in [('format','unknown'),('scene','other')]:
            with self.subTest(key=key),self.assertRaises(ValueError):
                self.profile[key]=value;self.bind()
            self.setUp()

    def test_missing_native_source_is_rejected(self):
        self.profile['rows'].pop()
        with self.assertRaisesRegex(ValueError,'source'):self.bind()

    def test_duplicate_source_and_target_identities_are_rejected(self):
        self.profile['rows'].append(copy.deepcopy(self.profile['rows'][0]))
        with self.assertRaisesRegex(ValueError,'[Dd]uplicate'):self.bind()
        self.setUp();self.level['lights'].append(copy.deepcopy(self.level['lights'][0]))
        with self.assertRaisesRegex(ValueError,'[Dd]uplicate'):self.bind()

    def test_invalid_strength_is_rejected_without_mutation(self):
        for invalid in [-1,float('inf'),float('nan'),True,'1',None,1e100]:
            self.setUp();self.profile['rows'][0]['fields']['specular_intensity']=invalid;before=copy.deepcopy(self.level)
            with self.subTest(value=invalid),self.assertRaises(ValueError):self.bind()
            self.assertEqual(self.level,before)

    def test_missing_field_and_invalid_identity_are_rejected(self):
        del self.profile['rows'][0]['fields']['specular_intensity']
        with self.assertRaises(ValueError):self.bind()
        self.setUp();self.level['lights'][0]['id']='native-light-invalid'
        with self.assertRaises(ValueError):self.bind()
        self.setUp();self.profile['rows'][0]['entity_id']=True
        with self.assertRaises(ValueError):self.bind()

    def test_cli_creates_new_level_and_receipt_and_preserves_source(self):
        with tempfile.TemporaryDirectory() as folder:
            root=Path(folder);level=root/'source.azurelevel';profile=root/'profiles.json';output=root/'bound.azurelevel';receipt=root/'receipt.json'
            level.write_text(json.dumps(self.level));profile.write_text(json.dumps(self.profile));before=level.read_bytes()
            command=[sys.executable,str(Path(__file__).with_name('bind_native_light_profiles.py')),
                '--level',str(level),'--profiles',str(profile),'--source-scene','fixture','--output',str(output),'--receipt',str(receipt)]
            subprocess.run(command,check=True,capture_output=True)
            self.assertEqual(level.read_bytes(),before);self.assertEqual(json.loads(receipt.read_text())['binding']['boundLights'],2)
            saved=output.read_bytes();repeated=subprocess.run(command,capture_output=True)
            self.assertNotEqual(repeated.returncode,0);self.assertEqual(output.read_bytes(),saved)


if __name__=='__main__':unittest.main()
