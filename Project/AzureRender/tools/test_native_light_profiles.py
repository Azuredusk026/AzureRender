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
from endfield_native.light_profiles import bind_captured_lights


class NativeLightProfileTests(unittest.TestCase):
    def capture_fixture(self):
        raw=bytearray((6+256*8)*16)
        for index,mode in [(0,16),(1,0)]:
            rows=[(.5,.25,1,1),(2,3,4,.1),(0,0,0,2),(-1,0,0,0),(1,.8,0,0),(0,0,0,0),(0,0,0,2),(0,0,.5,-1)]
            for row,values in enumerate(rows):struct.pack_into('<4f',raw,(6+index*8+row)*16,*values)
            struct.pack_into('<I',raw,(6+index*8+3)*16+12,mode)
        selection=dict(schemaVersion=1,sourceScene='fixture',event=17,namespace='frame17',indices=[0,1],origin=[1,0,0],
            tileWords=[3]+[0]*7,depthWords=[3]+[0]*7,surfaceReceivers=True,replaceLightIds=['native-light-7','native-light-9'])
        level=copy.deepcopy(self.level);level['nodes']=[]
        for light in level['lights']:
            light['nodeId']=light['id'];level['nodes'].append(dict(id=light['id']))
        return level,raw,selection

    def test_captured_selection_binds_world_nodes_receivers_and_retains_sun(self):
        level,raw,selection=self.capture_fixture();before=copy.deepcopy(level)
        result,receipt=bind_captured_lights(level,raw,selection,source_scene='fixture')
        self.assertEqual(level,before);self.assertEqual(result['lights'][0],before['lights'][2])
        self.assertEqual(receipt['indices'],[0,1]);self.assertEqual(receipt['replacedLights'],2)
        first,second=result['lights'][1:]
        self.assertEqual(first['id'],'frame17-0');self.assertEqual(first['color'],[.5,.25,1]);self.assertEqual(first['intensity'],1)
        self.assertEqual(first['shape']['receivers'],dict(schemaVersion=1,surfaces=True,characters=False))
        self.assertEqual(second['shape']['receivers'],dict(schemaVersion=1,surfaces=True,characters=True))
        self.assertAlmostEqual(second['shape']['characterResponse']['diffuseFloor'],.25)
        self.assertEqual(result['nodes'][-2]['components']['azure.transform']['data']['translation'],[-1,3,4])
        self.assertEqual(result['nodes'][-2]['parentId'],'')

    def test_captured_selection_rejects_stale_indices_identity_and_scene(self):
        for mutate in [lambda l,s:s.update(indices=[2]),lambda l,s:s.update(indices=[0,0]),lambda l,s:s.update(sourceScene='other'),
                       lambda l,s:s.update(tileWords=[True]+[0]*7),lambda l,s:s.update(replaceLightIds=['missing']),
                       lambda l,s:l['nodes'].append(dict(id='frame17-0')),lambda l,s:s.update(namespace='../escape'),
                       lambda l,s:s.update(surfaceReceivers=1),lambda l,s:s.update(origin=None)]:
            level,raw,selection=self.capture_fixture();mutate(level,selection)
            with self.assertRaises(ValueError):bind_captured_lights(level,raw,selection,source_scene='fixture')

    def test_inactive_records_are_rejected_before_decoding_unused_bytes(self):
        level,raw,selection=self.capture_fixture();selection['indices']=[2]
        with self.assertRaisesRegex(ValueError,'active masks'):bind_captured_lights(level,raw,selection,source_scene='fixture')

    def test_captured_cli_writes_snapshot_and_explicit_source_receipt(self):
        level,raw,selection=self.capture_fixture()
        with tempfile.TemporaryDirectory() as folder:
            root=Path(folder);source=root/'source.azurelevel';constants=root/'constants.bin';selected=root/'selection.json'
            source.write_text(json.dumps(level));constants.write_bytes(raw);selected.write_text(json.dumps(selection));before=source.read_bytes()
            output=root/'snapshot.azurelevel';receipt=root/'receipt.json'
            command=[sys.executable,str(Path(__file__).with_name('bind_native_light_profiles.py')),'--level',str(source),
                '--captured-constants',str(constants),'--selection',str(selected),'--source-scene','fixture','--output',str(output),'--receipt',str(receipt)]
            completed=subprocess.run(command,check=True,capture_output=True,text=True)
            self.assertEqual(json.loads(completed.stdout)['createdLights'],2)
            saved=json.loads(receipt.read_text());self.assertEqual(saved['schemaVersion'],2)
            self.assertEqual(saved['binding']['indices'],[0,1]);self.assertIn('sha256',saved['capturedConstants'])
            self.assertEqual(source.read_bytes(),before);self.assertEqual(len(json.loads(output.read_text())['lights']),3)

    def test_captured_selection_rejects_unavailable_consumers(self):
        for row,component,value,integer in [(3,3,3,True),(7,3,1,False),(2,2,1,False)]:
            level,raw,selection=self.capture_fixture()
            struct.pack_into('<I' if integer else '<f',raw,(6+row)*16+component*4,value)
            with self.assertRaises(ValueError):bind_captured_lights(level,raw,selection,source_scene='fixture')

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

    def test_explicit_srgb_color_uses_captured_float32_operations(self):
        self.profile['rows'][0]['fields'].update(color=[1,.9753895998001099,.9528301358222961,1],use_color_temperature=False)
        self.profile['rows'][1]['fields'].update(color=[.5,.04,1,1],use_color_temperature=False)
        before=copy.deepcopy(self.level)
        result,receipt=bind_specular_profiles(self.level,self.profile,source_scene='fixture',color_encoding='authored-srgb')
        color=result['lights'][0]['color'];f=lambda x:struct.unpack('<f',struct.pack('<f',x))[0]
        self.assertEqual([f(c*131.60000610351562) for c in color],[131.60000610351562,124.35221099853516,117.91789245605469])
        self.assertAlmostEqual(result['lights'][1]['color'][0],.21404114,places=7)
        self.assertAlmostEqual(result['lights'][1]['color'][1],.003095975,places=8)
        self.assertEqual(receipt['coloredLights'],2);self.assertEqual(receipt['unresolvedColorLights'],[])
        self.assertEqual(result['lights'][0]['intensity'],3);self.assertEqual(self.level,before)
        repeated,_=bind_specular_profiles(result,self.profile,source_scene='fixture',color_encoding='authored-srgb')
        self.assertEqual(result,repeated)

    def test_temperature_colors_are_explicitly_unresolved_and_retained(self):
        for row in self.profile['rows']:row['fields'].update(color=[.5,.25,1,1],use_color_temperature=False)
        self.profile['rows'][1]['fields']['use_color_temperature']=True
        self.level['lights'][1]['color']=[.9,.8,.7]
        result,receipt=bind_specular_profiles(self.level,self.profile,source_scene='fixture',color_encoding='authored-srgb')
        self.assertEqual(result['lights'][1]['color'],[.9,.8,.7]);self.assertEqual(receipt['unresolvedColorLights'],['native-light-9'])
        self.assertEqual(receipt['coloredLights'],1)

    def test_color_policy_and_invalid_inputs_are_rejected(self):
        for invalid in [None,[-1,0,0],[float('nan'),0,0],[2,0,0],[True,0,0],[0,0]]:
            self.setUp()
            for row in self.profile['rows']:row['fields'].update(color=[1,1,1],use_color_temperature=False)
            self.profile['rows'][0]['fields']['color']=invalid
            with self.subTest(color=invalid),self.assertRaises(ValueError):
                bind_specular_profiles(self.level,self.profile,source_scene='fixture',color_encoding='authored-srgb')
        with self.assertRaises(ValueError):bind_specular_profiles(self.level,self.profile,source_scene='fixture',color_encoding='unknown')

    def test_linear_color_policy_retains_linear_values(self):
        for row in self.profile['rows']:row['fields'].update(color=[2,.25,1],use_color_temperature=False)
        result,receipt=bind_specular_profiles(self.level,self.profile,source_scene='fixture',color_encoding='linear')
        self.assertEqual(result['lights'][0]['color'],[2,.25,1]);self.assertEqual(receipt['coloredLights'],2)

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
            for row in self.profile['rows']:row['fields'].update(color=[.5,.25,1,1],use_color_temperature=False)
            self.profile['rows'][1]['fields']['use_color_temperature']=True
            profile.write_text(json.dumps(self.profile))
            color_command=command.copy();color_output=root/'color.azurelevel';color_receipt=root/'color-receipt.json'
            color_command[color_command.index('--output')+1]=str(color_output)
            color_command[color_command.index('--receipt')+1]=str(color_receipt)
            completed=subprocess.run(color_command+['--profile-color-encoding','authored-srgb'],check=True,capture_output=True,text=True)
            summary=json.loads(completed.stdout);self.assertEqual(summary['coloredLights'],1)
            self.assertEqual(summary['unresolvedColorLights'],['native-light-9'])
            self.assertEqual(json.loads(color_receipt.read_text())['binding']['colorEncoding'],'authored-srgb')


if __name__=='__main__':unittest.main()
