import copy
import math
import struct
import unittest

try:
    from bake_animation_loop import append_loop_clip
except ImportError:
    append_loop_clip = None


def fixture(path='translation', interpolation='LINEAR'):
    times=[0, .5, .9, 1]
    vectors=[[0,0,0],[.1,0,0],[.3,1,0],[.6,1,0]]
    kind='VEC3'
    if path=='rotation':
        vectors=[[0,0,0,1],[0,0,0,1],[0,0,math.sqrt(.5),math.sqrt(.5)],[0,0,math.sqrt(.75),.5]];kind='VEC4'
    raw=struct.pack('<4f',*times);offset=len(raw);flat=[x for v in vectors for x in v];raw+=struct.pack('<'+'f'*len(flat),*flat)
    doc=dict(asset=dict(version='2.0'),buffers=[dict(byteLength=len(raw))],bufferViews=[dict(buffer=0,byteOffset=0,byteLength=offset),dict(buffer=0,byteOffset=offset,byteLength=len(raw)-offset)],accessors=[dict(bufferView=0,componentType=5126,count=4,type='SCALAR'),dict(bufferView=1,componentType=5126,count=4,type=kind)],nodes=[dict(name='animated-prop')],animations=[dict(name='source',samplers=[dict(input=0,output=1,interpolation=interpolation)],channels=[dict(sampler=0,target=dict(node=0,path=path))])])
    return doc,raw


def output_values(doc,raw,index):
    a=doc['accessors'][index];v=doc['bufferViews'][a['bufferView']];w={'SCALAR':1,'VEC3':3,'VEC4':4}[a['type']];flat=struct.unpack_from('<'+'f'*(a['count']*w),raw,v['byteOffset']);return [list(flat[i:i+w]) for i in range(0,len(flat),w)]


def samples(doc,raw,sampler):
    return [v[0] for v in output_values(doc,raw,sampler['input'])],output_values(doc,raw,sampler['output'])


def at(times,values,t):
    if t<=times[0]:return values[0]
    for i in range(len(times)-1):
        if times[i]<=t<=times[i+1]:
            a=(t-times[i])/(times[i+1]-times[i]);return [(1-a)*x+a*y for x,y in zip(values[i],values[i+1])]
    return values[-1]


class NativeLoopTests(unittest.TestCase):
    def setUp(self):self.assertIsNotNone(append_loop_clip,'versioned loop clip baker missing')

    def test_appends_a_closed_cycle_preserving_source_and_timing(self):
        doc,raw=fixture();before=copy.deepcopy(doc);out,binary,receipt=append_loop_clip(doc,raw,0,'closed',.2)
        self.assertEqual(doc,before);self.assertEqual(binary[:len(raw)],raw);self.assertEqual(out['animations'][0],before['animations'][0])
        self.assertEqual(receipt['clipIndex'],1);self.assertEqual(receipt['duration'],1)
        sampler=out['animations'][1]['samplers'][0];times,values=samples(out,binary,sampler)
        self.assertEqual(values[0],values[-1]);self.assertAlmostEqual(at(times,values,.5)[0],.1,places=6)
        self.assertAlmostEqual(at(times,values,.9)[0],.15,places=5);self.assertAlmostEqual(at(times,values,.9)[1],.5,places=5)

    def test_rotation_uses_normalized_shortest_arc(self):
        doc,raw=fixture('rotation');out,binary,receipt=append_loop_clip(doc,raw,0,'closed',.2)
        times,values=samples(out,binary,out['animations'][1]['samplers'][0]);self.assertEqual(values[0],values[-1])
        for q in values:self.assertAlmostEqual(sum(x*x for x in q),1,places=6)
        q=at(times,values,.9);angle=math.degrees(2*math.atan2(q[2],q[3]));self.assertAlmostEqual(angle,45,places=4)

    def test_constant_tracks_keep_their_pose(self):
        doc,raw=fixture();doc['accessors'][0]['count']=1;doc['accessors'][1]['count']=1
        out,binary,receipt=append_loop_clip(doc,raw,0,'closed',.2)
        self.assertEqual(output_values(out,binary,out['animations'][1]['samplers'][0]['output']),[[0,0,0]])

    def test_closed_step_tracks_preserve_authored_discrete_samples(self):
        doc,raw=fixture(interpolation='STEP');raw=raw[:-12]+raw[16:28]
        out,binary,receipt=append_loop_clip(doc,raw,0,'closed',.2)
        sampler=out['animations'][1]['samplers'][0]
        self.assertEqual(sampler['interpolation'],'STEP')
        self.assertEqual(output_values(out,binary,sampler['output']),output_values(doc,raw,1))

    def test_step_reset_changes_only_the_loop_endpoint(self):
        doc,raw=fixture(interpolation='STEP');out,binary,receipt=append_loop_clip(doc,raw,0,'closed',.2)
        sampler=out['animations'][1]['samplers'][0];times,values=samples(out,binary,sampler)
        self.assertEqual(sampler['interpolation'],'STEP')
        self.assertEqual(values[:-1],output_values(doc,raw,1)[:-1])
        self.assertEqual(values[-1],values[0]);self.assertEqual(times[-1],1)

    def test_leading_hold_and_pre_window_interpolation_are_preserved(self):
        doc,raw=fixture();raw=struct.pack('<f',.1)+raw[4:]
        out,binary,receipt=append_loop_clip(doc,raw,0,'closed',.2)
        times,values=samples(out,binary,out['animations'][1]['samplers'][0])
        self.assertEqual(at(times,values,0),[0,0,0])
        self.assertAlmostEqual(at(times,values,.79)[0],.245,places=5)
        self.assertAlmostEqual(at(times,values,.79)[1],.725,places=5)

    def test_shorter_track_closes_at_the_common_clip_duration(self):
        doc,raw=fixture();offset=len(raw);raw+=struct.pack('<4f',0,.25,.45,.5);doc['buffers'][0]['byteLength']=len(raw)
        doc['bufferViews'].append(dict(buffer=0,byteOffset=offset,byteLength=16));doc['accessors'].append(dict(bufferView=2,componentType=5126,count=4,type='SCALAR'))
        doc['nodes'].append(dict(name='second-part'));doc['animations'][0]['samplers'].append(dict(input=2,output=1,interpolation='LINEAR'));doc['animations'][0]['channels'].append(dict(sampler=1,target=dict(node=1,path='translation')))
        out,binary,receipt=append_loop_clip(doc,raw,0,'closed',.2);times,values=samples(out,binary,out['animations'][1]['samplers'][1])
        self.assertAlmostEqual(at(times,values,.5)[0],.6,places=6)
        self.assertEqual(at(times,values,1),[0,0,0])

    def test_preserves_the_source_time_origin_and_duration(self):
        doc,raw=fixture();raw=struct.pack('<f',.1)+raw[4:]
        out,binary,receipt=append_loop_clip(doc,raw,0,'closed',.2);times,values=samples(out,binary,out['animations'][1]['samplers'][0])
        self.assertAlmostEqual(times[0],.1,places=6);self.assertAlmostEqual(receipt['duration'],.9,places=6)

    def test_float32_boundary_keys_are_unique_after_serialization(self):
        doc,raw=fixture();out,binary,receipt=append_loop_clip(doc,raw,0,'closed',.10000001)
        times,values=samples(out,binary,out['animations'][1]['samplers'][0])
        self.assertTrue(all(a<b for a,b in zip(times,times[1:])),times)

    def test_rejects_unsupported_curves_invalid_window_and_quaternion(self):
        for change in ['cubic','window','quaternion']:
            doc,raw=fixture('rotation');window=.2
            if change=='cubic':doc['animations'][0]['samplers'][0]['interpolation']='CUBICSPLINE'
            elif change=='window':window=0
            else:raw=raw[:16]+b'\0'*16+raw[32:]
            with self.subTest(change=change),self.assertRaises(ValueError):append_loop_clip(doc,raw,0,'closed',window)


if __name__=='__main__':unittest.main()
