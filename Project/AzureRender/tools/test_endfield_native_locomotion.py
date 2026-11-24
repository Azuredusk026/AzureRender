import copy
import unittest
import inspect
from endfield_native.acl_timeline import build_sample_plan
from test_endfield_native_acl_timeline import stream
from endfield_native.locomotion_inventory import analyze_clip, append_animation, static_pose_override


class LocomotionTests(unittest.TestCase):
    def test_static_face_keeps_head_motion_and_rejects_missing_names(self):
        doc={'nodes':[{'name':'Head'},{'name':'eye'}], 'animations':[{'channels':[
            {'target':{'node':0,'path':'rotation'}}, {'target':{'node':1,'path':'rotation'}}]}]}
        result,report=static_pose_override(doc,['eye'])
        self.assertEqual(len(result['animations'][0]['channels']),1)
        self.assertEqual(result['animations'][0]['channels'][0]['target']['node'],0)
        self.assertEqual(report['removed_channels'],1)
        with self.assertRaises(ValueError):static_pose_override(doc,['absent'])

    def test_append_by_node_identity_preserves_idle(self):
        import struct
        tail=struct.pack('<I4s',4,b'BIN\0')+b'1234'
        base={'nodes':[{'name':'root'},{'name':'joint'}], 'buffers':[{'byteLength':4}], 'animations':[{'name':'idle'}]}
        source={'nodes':[{'name':'joint'},{'name':'root'}], 'bufferViews':[{'buffer':0,'byteOffset':0,'byteLength':4}],
                'accessors':[{'bufferView':0,'componentType':5126,'count':1,'type':'SCALAR'}],
                'animations':[{'name':'walk','samplers':[{'input':0,'output':0}], 'channels':[{'sampler':0,'target':{'node':0,'path':'translation'}}]}]}
        result,newtail=append_animation(base,tail,source,tail,'walk')
        self.assertEqual(result['animations'][0],{'name':'idle'})
        self.assertEqual(result['animations'][1]['channels'][0]['target']['node'],1)
        self.assertEqual(newtail[8:],b'12341234')
        self.assertEqual(len(base['animations']),1)
        source['nodes'][0]['translation']=[1,0,0]
        with self.assertRaisesRegex(ValueError,'rest'):append_animation(base,tail,source,tail,'walk')

    def fixture(self):
        rest = {'position': [0, 0, 0], 'rotation': [0, 0, 0, 1], 'scale': [1, 1, 1]}
        hierarchy = [{'index': 0, 'path': 'root', 'parent_index': -1, 'rest_local': rest}]
        frames = [{'time': t, 'locals': [dict(rest, position=[0, 0, 2*t])]} for t in [0, .5, 1]]
        clip = {'version': 10, 'source': 'cab', 'path_id': 2, 'start': 0, 'stop': 1, 'm_name': 'walk'}
        return clip, hierarchy, frames, ['root']

    def test_motion_and_loop_diagnostics(self):
        result = analyze_clip(*self.fixture())
        self.assertEqual(result['root']['displacement_m'], [0, 0, 2])
        self.assertEqual(result['root']['mean_speed_mps'], 2)
        self.assertEqual(result['root']['kind'], 'moving_root')
        self.assertFalse(result['loop']['authored_loop_verified'])

    def test_explicit_decoder_domain_keeps_the_unverified_game_clock_visible(self):
        c,h,f,p=self.fixture();c['stop']=1.02
        plan=build_sample_plan(dict(root_motion=stream(2,True,2),float=stream(3,False,2),
            transform=stream(3,False,2,track_type=12)),source_start=0,source_stop=1.02)
        self.assertIn('decoder_timeline',inspect.signature(analyze_clip).parameters,
            'Inventory must accept a separately evidenced decoder time domain')
        with self.assertRaisesRegex(ValueError,'domain'):analyze_clip(c,h,f,p)
        result=analyze_clip(c,h,f,p,decoder_timeline=plan)
        self.assertEqual(result['time_domain']['kind'],'decoder_seconds')
        self.assertFalse(result['time_domain']['game_time_mapping_verified'])
        self.assertEqual(result['time_domain']['source_clip_interval_seconds'],[0,1.02])
        plan['sample_times'][-1]=.9
        with self.assertRaisesRegex(ValueError,'domain'):analyze_clip(c,h,f,p,decoder_timeline=plan)

    def test_reject_missing_bone(self):
        c, h, f, p = self.fixture()
        with self.assertRaisesRegex(ValueError, 'skeleton'): analyze_clip(c, h, f, p + ['absent'])

    def test_decoder_plan_rejects_invalid_numeric_domains(self):
        c,h,f,p=self.fixture()
        plan=build_sample_plan(dict(root_motion=stream(2,True,2),float=stream(3,False,2),
            transform=stream(3,False,2,track_type=12)),source_start=0,source_stop=1)
        for field,value in [('source_clip_domain',[False,1]),('source_clip_domain',None),
                            ('sample_times',[0,'bad',1]),('sample_times',None),
                            ('decoder_duration',True),('decoder_duration','1')]:
            candidate=copy.deepcopy(plan);candidate[field]=value
            with self.subTest(field=field,value=value):
                with self.assertRaises(ValueError):
                    analyze_clip(c,h,f,p,decoder_timeline=candidate)

    def test_reject_version_and_duration(self):
        c, h, f, p = self.fixture(); c['version'] = 11
        with self.assertRaisesRegex(ValueError, 'version'): analyze_clip(c, h, f, p)
        c['version'] = 10; f[1]['time'] = 0
        with self.assertRaisesRegex(ValueError, 'time'): analyze_clip(c, h, f, p)

    def test_reject_nonfinite_pose_and_incomplete_domain(self):
        c, h, f, p = self.fixture(); f[1]['locals'][0]['position'][0] = float('nan')
        with self.assertRaisesRegex(ValueError, 'finite'): analyze_clip(c, h, f, p)
        c, h, f, p = self.fixture(); c['stop'] = 2
        with self.assertRaisesRegex(ValueError, 'domain'): analyze_clip(c, h, f, p)

    def test_authored_loop_requires_matching_object(self):
        c,h,f,p = self.fixture()
        evidence = {'cab':'cab', 'pathId':2, 'm_LoopTime':True, 'start':0, 'stop':1}
        result = analyze_clip(c,h,f,p,loop_source=evidence)
        self.assertTrue(result['loop']['authored_loop_verified'])
        evidence['pathId'] = 3
        with self.assertRaisesRegex(ValueError,'identity'): analyze_clip(c,h,f,p,loop_source=evidence)


if __name__ == '__main__': unittest.main()
