import copy
import json
from pathlib import Path
import struct
import subprocess
import sys
import tempfile
import unittest

from endfield_native.light_profiles import bind_specular_profiles
from endfield_native.light_profiles import bind_directional_radiometry


class NativeLightProfileTests(unittest.TestCase):
    def main_header(self):
        raw=bytearray(96)
        struct.pack_into('<4f',raw,0,0,-1,0,0)
        struct.pack_into('<4f',raw,16,2,3,4,4)
        struct.pack_into('<4f',raw,64,.195,0,1,0)
        return raw

    def test_main_radiometry_preserves_local_direction_and_other_lights(self):
        before=copy.deepcopy(self.level)
        self.level['lights'][2]['shape']['direction']=[1,0,0]
        result,receipt=bind_directional_radiometry(self.level,self.main_header(),light_id='project-sun',enabled=True)
        light=result['lights'][2]
        self.assertEqual(light['color'],[.5,.75,1]);self.assertEqual(light['intensity'],4)
        self.assertAlmostEqual(light['shape']['specularScale'],.195,places=7)
        self.assertEqual(light['shape']['direction'],[1,0,0]);self.assertTrue(light['enabled'])
        self.assertEqual(result['lights'][:2],before['lights'][:2])
        self.assertEqual(receipt['sourceDirection'],[0,-1,0]);self.assertEqual(receipt['directionBinding'],'existing-local-direction')

    def test_main_radiometry_requires_directional_identity_and_explicit_activation(self):
        for identity,enabled in [('missing',True),('native-light-7',True),('project-sun',1)]:
            with self.subTest(identity=identity,enabled=enabled),self.assertRaises(ValueError):
                bind_directional_radiometry(self.level,self.main_header(),light_id=identity,enabled=enabled)

    def test_main_header_rejects_length_nonfinite_radiometry_and_invalid_direction(self):
        variants=[self.main_header()[:-1]]
        for offset,value in [(0,float('nan')),(4,0),(16,-1),(20,float('inf')),(64,-1)]:
            raw=self.main_header();struct.pack_into('<f',raw,offset,value);variants.append(raw)
        for raw in variants:
            with self.subTest(header=raw.hex()),self.assertRaises(ValueError):
                bind_directional_radiometry(self.level,raw,light_id='project-sun',enabled=True)

    def test_zero_main_radiometry_and_disabled_activation_are_valid(self):
        raw=self.main_header();struct.pack_into('<4f',raw,16,0,0,0,0)
        result,_=bind_directional_radiometry(self.level,raw,light_id='project-sun',enabled=False)
        self.assertEqual(result['lights'][2]['color'],[0,0,0]);self.assertEqual(result['lights'][2]['intensity'],0)
        self.assertFalse(result['lights'][2]['enabled'])

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
            header=root/'main.bin';header.write_bytes(self.main_header())
            main_command=command.copy();main_output=root/'main.azurelevel';main_receipt=root/'main-receipt.json'
            main_command[main_command.index('--output')+1]=str(main_output)
            main_command[main_command.index('--receipt')+1]=str(main_receipt)
            partial=subprocess.run(main_command+['--main-header',str(header)],capture_output=True)
            self.assertNotEqual(partial.returncode,0);self.assertFalse(main_output.exists());self.assertFalse(main_receipt.exists())
            subprocess.run(main_command+['--main-header',str(header),'--main-light','project-sun','--main-enabled','0'],check=True,capture_output=True)
            self.assertEqual(json.loads(main_output.read_text())['lights'][2]['intensity'],4)
            self.assertFalse(json.loads(main_receipt.read_text())['mainBinding']['enabled'])
            self.assertEqual(level.read_bytes(),before)


if __name__=='__main__':unittest.main()
