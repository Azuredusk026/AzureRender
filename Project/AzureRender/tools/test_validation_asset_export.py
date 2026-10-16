import copy
import unittest
import os
import validation_asset as v


class ValidationExportTests(unittest.TestCase):
    @unittest.skipUnless(os.environ.get('AZURE_BLENDER'), 'Set AZURE_BLENDER for real importer time regression')
    def test_blender_seconds_match_imported_keyframes(self):
        import base64,json,struct,subprocess,tempfile
        from pathlib import Path
        binary=bytearray();views=[];accessors=[]
        def add(values,kind,count,component=5126):
            binary.extend(b'\0'*(-len(binary)%4));start=len(binary)
            binary.extend(struct.pack('<'+('f' if component==5126 else 'H')*len(values),*values))
            views.append({'buffer':0,'byteOffset':start,'byteLength':len(binary)-start})
            accessors.append({'bufferView':len(views)-1,'componentType':component,'count':count,'type':kind})
            return len(accessors)-1
        position=add([0,0,0,0,1,0,0,0,1],'VEC3',3)
        joints=add([0]*12,'VEC4',3,5123);weights=add([1,0,0,0]*3,'VEC4',3)
        times=add([0,1],'SCALAR',2);values=add([0,0,0,1,0,0],'VEC3',2)
        doc={'asset':{'version':'2.0'},'buffers':[{'byteLength':len(binary),'uri':'data:application/octet-stream;base64,'+base64.b64encode(binary).decode()}],
             'bufferViews':views,'accessors':accessors,'nodes':[{'name':'rig','children':[1]},{'name':'joint'},{'name':'mesh','mesh':0,'skin':0}],
             'skins':[{'joints':[1]}],'meshes':[{'primitives':[{'attributes':{'POSITION':position,'JOINTS_0':joints,'WEIGHTS_0':weights}}]}],
             'scenes':[{'nodes':[0,2]}],'scene':0,'animations':[{'name':'idle','samplers':[{'input':times,'output':values,'interpolation':'LINEAR'}],
             'channels':[{'sampler':0,'target':{'node':1,'path':'translation'}}]}]}
        runs=Path(__file__).resolve().parents[1]/'build/runs';runs.mkdir(parents=True,exist_ok=True)
        with tempfile.TemporaryDirectory(prefix='blender-time-test-',dir=runs) as temp:
            root=Path(temp);source=root/'linear.gltf';source.write_text(json.dumps(doc));output=root/'reference.json'
            subprocess.run([os.environ['AZURE_BLENDER'],'--background','--factory-startup','--python',str(Path(__file__).parent/'blender/export_validation_asset.py'),
                            '--','--input',str(source),'--output',str(output),'--duration','1','--clip','idle'],check=True,capture_output=True)
            reference=json.loads(output.read_text())
            for sample in reference['samples']:
                self.assertAlmostEqual(sample['bones']['joint'][0][3],sample['time'],places=5)

    def fixture(self):
        return {'nodes': [{'name': 'root', 'children': [1]}, {'name': 'joint'}],
                'skins': [{'joints': [0, 1]}]}

    def test_bind_difference_refuses_merge(self):
        a = self.fixture(); b = copy.deepcopy(a)
        b['nodes'][1]['translation'] = [1, 0, 0]
        with self.assertRaisesRegex(ValueError, 'fingerprint'):
            v.match_skeletons(a, b'', b, b'')

    def test_inverse_bind_difference_refuses_merge(self):
        import struct
        a=self.fixture(); a['skins'][0]['inverseBindMatrices']=0
        a['accessors']=[{'bufferView':0,'componentType':5126,'type':'MAT4','count':2}]
        a['bufferViews']=[{'byteLength':128}]
        values=[1.,0,0,0,0,1,0,0,0,0,1,0,0,0,0,1]*2
        raw=struct.pack('<32f',*values); values[12]=.1
        with self.assertRaisesRegex(ValueError,'fingerprint'):
            v.match_skeletons(a,raw,a,struct.pack('<32f',*values))

    def test_native_provenance_cannot_be_claimed(self):
        with self.assertRaisesRegex(ValueError,'Native provenance'):
            v.assemble('missing-target','missing-source','idle','missing-output','game-native')

    def test_hierarchy_difference_refuses_merge(self):
        a = self.fixture(); b = copy.deepcopy(a); b['nodes'][0]['children'] = []
        with self.assertRaisesRegex(ValueError, 'fingerprint'):
            v.match_skeletons(a, b'', b, b'')

    def test_matching_skeleton_maps_nodes(self):
        a = self.fixture()
        self.assertEqual(v.match_skeletons(a, b'', a, b''), {0: 0, 1: 1})

    def test_unnamed_nodes_have_stable_index_identity(self):
        a = self.fixture(); a['nodes'].append({'mesh': 0})
        self.assertEqual(v.match_skeletons(a, b'', a, b'')[2], 2)

    def test_missing_curve_refused(self):
        with self.assertRaisesRegex(ValueError, 'clip'):
            v.select_clip({}, 'idle')

    def test_normalized_weight_accessor(self):
        doc = {'accessors':[{'bufferView':0,'componentType':5121,'type':'VEC4','count':1,'normalized':True}],
               'bufferViews':[{'byteLength':4}]}
        self.assertEqual(v.accessor(doc, bytes([255,0,0,0]),0).tolist(), [[1,0,0,0]])

    def test_weight_restore_requires_same_topology(self):
        a={'meshes':[{'primitives':[]}]}; b={'meshes':[]}
        with self.assertRaisesRegex(ValueError,'topology'):
            v.compatible_geometry(a,b'',b,b'')

    def test_sample_grid_contains_endpoint_and_half_frames(self):
        times = v.reference_times(1.0)
        self.assertEqual(times[0], 0)
        self.assertEqual(times[-1], 1)
        self.assertIn(0.5 + 1/120, times)

    def test_rotation_bake_preserves_unit_midpoint(self):
        import numpy as np
        values=v.resample([0,1],[[0,0,0,1],[0,0,1,0]],[.5],rotation=True)
        self.assertTrue(np.allclose(values[0],[0,0,2**-.5,2**-.5]))

    def test_step_bake_preserves_previous_value(self):
        self.assertEqual(v.resample([0,1],[[0],[4]],[.5,1],interpolation='STEP').tolist(),[[0],[4]])

    def test_comparator_detects_translation_error(self):
        from compare_deformation_reference import compare
        a = {'space': 'gltf-world', 'units': 'metres', 'samples': [{'time': 0, 'vertices': {'v': [0,0,0]}, 'bones': {}}]}
        b = copy.deepcopy(a); b['samples'][0]['vertices']['v'][0] = .002
        self.assertFalse(compare(a, b)['passed'])

    def test_comparator_rejects_missing_sample(self):
        from compare_deformation_reference import compare
        a = {'space': 'gltf-world', 'units': 'metres', 'samples': []}
        with self.assertRaises(ValueError): compare(a, a)


if __name__ == '__main__': unittest.main()
