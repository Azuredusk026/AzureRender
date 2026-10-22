"""Pin captured light bytes, independent mode bits and public emitter parameters."""
import math
import struct
import unittest
from endfield_native import captured_lights as subject


class CapturedLightTests(unittest.TestCase):
    def fixture(self,kind=1,mode=0):
        raw=bytearray((6+8*256)*16);offset=6*16
        rows=((.35,.3,.25,kind),(-299.9,301.5,-299.7,2),(.5,0,-1,2),
              (2,1,1,0),(1,.8,0,0),(0,0,0,0),(0,0,0,2),(.8,0,1,-1))
        for i,row in enumerate(rows):struct.pack_into('<4f',raw,offset+i*16,*row)
        struct.pack_into('<I',raw,offset+3*16+12,mode)
        return raw

    def decode(self,raw,indices=(0,)):
        return subject.decode_records(raw,indices,origin=(-300,300,-300))

    def test_bitcast_mode_and_reflected_camera_relative_emitter(self):
        light=self.decode(self.fixture(mode=4))[0]
        self.assertEqual(light['mode'],4)
        self.assertEqual(light['composition'],'replace')
        self.assertEqual(light['shape']['type'],'point')
        self.assertAlmostEqual(light['radius'],.5)
        self.assertAlmostEqual(light['position'][0],-.1,places=4)
        self.assertAlmostEqual(light['position'][1],1.5)
        self.assertAlmostEqual(light['position'][2],.3,places=4)

    def test_default_mode_keeps_independent_wrap_and_compression(self):
        light=self.decode(self.fixture())[0]
        self.assertEqual(light['composition'],'add')
        self.assertAlmostEqual(light['response']['diffuseFloor'],.25)
        self.assertAlmostEqual(light['response']['compression'],.8)
        self.assertEqual(light['shape']['falloff'],'range-power')
        self.assertEqual(light['shape']['falloffExponent'],2)

    def test_spot_octahedral_axis_and_cone(self):
        raw=self.fixture(kind=0);struct.pack_into('<4f',raw,(6+2)*16,.5,0,.5,4)
        light=self.decode(raw)[0];shape=light['shape']
        self.assertEqual(shape['type'],'spot')
        self.assertEqual(shape['direction'],[0,0,1])
        self.assertAlmostEqual(shape['outerCone'],math.pi/3)
        self.assertAlmostEqual(shape['innerCone'],math.acos(.75))

    def test_linear_emitter_and_source_dependencies_are_explicit(self):
        raw=self.fixture();struct.pack_into('<f',raw,(6+2)*16+8,1)
        light=self.decode(raw)[0]
        self.assertEqual(light['linearLength'],1)
        self.assertIn('linear-emitter',light['requiredCapabilities'])
        light=self.decode(self.fixture(mode=3))[0]
        self.assertIn('screen-depth',light['requiredCapabilities'])

    def test_invalid_buffers_indices_and_semantic_fields(self):
        for raw,indices in ((self.fixture()[:-1],(0,)),(self.fixture(),(-1,)),(self.fixture(),(256,)),(self.fixture(),(True,)),(self.fixture(),(0,0))):
            with self.assertRaises(ValueError):self.decode(raw,indices)
        for row,component,value in ((1,3,0),(0,0,float('nan')),(2,0,float('nan')),(4,1,2),(0,3,4)):
            raw=self.fixture();struct.pack_into('<f',raw,(6+row)*16+component*4,value)
            with self.assertRaises(ValueError):self.decode(raw)

    def test_half_packed_box_preserves_relative_translation_and_handedness(self):
        raw=self.fixture();packed=struct.pack('<12e',2,0,0,-.1,0,3,0,.2,0,0,4,-.3)
        raw[(6+5)*16:(6+5)*16+12]=packed[:12];raw[(6+6)*16:(6+6)*16+12]=packed[12:]
        struct.pack_into('<f',raw,(6+5)*16+12,1)
        light=self.decode(raw)[0];box=light['shape']['influenceBox']
        self.assertTrue(box['enabled'])
        for row,value in zip(box['worldToBox'],(-.1,.2,-.3)):
            point=sum(a*b for a,b in zip(row,light['position']+[1]))
            self.assertAlmostEqual(point,value,places=3)
        struct.pack_into('<12x',raw,(6+5)*16)
        struct.pack_into('<12x',raw,(6+6)*16)
        with self.assertRaises(ValueError):self.decode(raw)

    def test_default_radiance_matches_literal_captured_light_zero(self):
        color=subject.default_character_light_color([.35,.31684333086,.27559432387],.753646,
            .26159626245,diffuse_floor=.25,compression=1,scene_visibility=1)
        for a,b in zip(color,[.28741902113,.26019087434,.22631728649]):self.assertAlmostEqual(a,b,places=7)
        compressed=subject.default_character_light_color([8,4,2],1,1,diffuse_floor=0,compression=1,scene_visibility=1)
        self.assertEqual(compressed,[4/3,2/3,1/3])


if __name__=='__main__':unittest.main()
