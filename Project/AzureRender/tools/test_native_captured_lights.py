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

    def test_finite_line_direction_and_energy_have_independent_reference(self):
        result=subject.line_emitter_response([0,0,2],[1,0,0],2)
        self.assertEqual(result['direction'],[0,0,1])
        self.assertAlmostEqual(result['factor'],2/(5*math.sqrt(5)),places=12)
        point=subject.line_emitter_response([0,1,2],[1,0,0],0)
        self.assertAlmostEqual(point['factor'],1/6)
        for value in subject.line_emitter_response([2,0,0],[1,0,0],2)['direction']:self.assertTrue(math.isfinite(value))

    def test_line_response_checks_direction_and_length(self):
        for incoming,axis,length in (([0,0,2],[0,0,0],1),([0,float('nan'),2],[1,0,0],1),([0,0,2],[1,0,0],-1)):
            with self.assertRaises(ValueError):subject.line_emitter_response(incoming,axis,length)

    def test_color_blend_uses_axial_distance_and_literal_ordered_mix(self):
        result=subject.color_blend_response([.050873,-.64279,-1.31818],[0,0,1],1.5,
            exponent=2,strength=.7,facing_weight=0,normal=[0,0,1],projected=True)
        self.assertAlmostEqual(result['weight'],.7*(1-(1.31818/1.5)**2)**2,places=12)
        self.assertEqual(result['direction'],[0,0,-1])
        self.assertAlmostEqual(result['weight'],.03630534,places=5)

    def test_color_blend_adapter_keeps_range_projection_and_source_order(self):
        raw=self.fixture(mode=4,kind=0);struct.pack_into('<4f',raw,(6+2)*16,.5,0,.5,4)
        struct.pack_into('<4f',raw,(6+4)*16,.7,1,1,0)
        light=self.decode(raw)[0];converted=subject.render_color_blend(light)
        self.assertEqual(converted['characterResponse'],dict(schemaVersion=3,composition='color-blend',
            strength=light['response']['parameters'][0],facingWeight=0,projectedDistance=True,order=0))
        self.assertEqual(converted['falloff'],'range-power');self.assertEqual(converted['falloffExponent'],2)
        with self.assertRaises(ValueError):subject.render_color_blend(self.decode(self.fixture())[0])

    def test_screen_rim_keeps_depth_window_and_projected_direction(self):
        # A positive source edge, independent from the renderer's screen sampling.
        result=subject.screen_rim_response([.6,.4,.2],.5,[.3,.1,.05],[.6,0,.8],[-.8,0,.6],
            [0,0,1],[1,0,0],4,3,surface_tint=.8)
        self.assertEqual(result,[0,0,0])
        result=subject.screen_rim_response([.6,.4,.2],.5,[.3,.1,.05],[.6,0,.8],[.8,0,.6],
            [0,0,1],[1,0,0],4,3,surface_tint=.8)
        expected=[c*.5*.6*(.5+.8*(b*.96-.5)) for c,b in zip([.6,.4,.2],[.3,.1,.05])]
        for a,b in zip(result,expected):self.assertAlmostEqual(a,b,places=12)
        self.assertEqual(subject.screen_rim_response([1,1,1],1,[1,1,1],[1,0,0],[1,0,0],[0,0,1],[1,0,0],3.05,3,surface_tint=1),[0,0,0])

    def test_special_light_adapter_preserves_explicit_consumers(self):
        raw=self.fixture(mode=3)
        struct.pack_into('<I',raw,(6+2)*16+12,0x3fffffff);struct.pack_into('<I',raw,(6+3)*16,0x3fffffff)
        light=self.decode(raw)[0];response=subject.render_special_character_response(light)
        self.assertEqual(response,dict(schemaVersion=4,composition='screen-rim',offsetScale=1,surfaceTint=light['response']['parameters'][1]))
        self.assertEqual(subject.render_special_character_response(self.decode(self.fixture(mode=2))[0]),dict(schemaVersion=4,composition='specular-only'))
        with self.assertRaises(ValueError):subject.render_special_character_response(self.decode(self.fixture())[0])

    def test_rim_adapter_rejects_missing_native_shadow_consumer(self):
        with self.assertRaises(ValueError):subject.render_special_character_response(self.decode(self.fixture(mode=3))[0])

    def test_render_response_keeps_default_and_line_versions_explicit(self):
        point=self.decode(self.fixture())[0]
        response=subject.render_character_response(point)
        self.assertEqual(response,dict(schemaVersion=1,diffuseFloor=.25,compression=point['response']['compression']))
        raw=self.fixture();struct.pack_into('<f',raw,(6+2)*16+8,1)
        line=self.decode(raw)[0]
        self.assertEqual(subject.render_character_response(line),dict(schemaVersion=2,diffuseFloor=.25,
            compression=line['response']['compression'],linearLength=1))
        with self.assertRaises(ValueError):subject.render_character_response(self.decode(self.fixture(mode=4))[0])


if __name__=='__main__':unittest.main()
