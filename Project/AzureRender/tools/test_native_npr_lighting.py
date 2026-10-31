"""Decode explicit NPR globals and object blends from a confirmed draw."""
import unittest
from endfield_native import npr_lighting as subject


class NprLightingTests(unittest.TestCase):
    def test_scene_settings_do_not_require_object_light_rows(self):
        g,_=self.fixture()
        expected=subject.decode_lighting(*self.fixture(),ambient_row=113,main_color_row=114)['settings']
        for index in [114,115,121]:del g[index]
        self.assertEqual(subject.decode_scene_settings(g,ambient_row=113),expected)

    def test_scene_context_keeps_its_own_direction_and_visibility(self):
        g,_=self.fixture()
        g[29]=[1,1,1,0];g[110]=[0,1,.7,1];g[111]=[0,0,0,0]
        g[113]=[1,1,1,1];g[116]=[0,1,0,0];g[117]=[.15,.6,1,0];g[122]=[0,0,1,0]
        self.assertEqual(subject.decode_scene_settings(g,ambient_row=113),dict(
            schemaVersion=1,enabled=True,ambientColor=[1,1,1],ambientDirection=[0,1,0],
            directionBias=.15,directionGain=.6,directionFloor=1,outdoorGain=1,
            shadowAmbientGain=1,ambientWeight=1,indoorMix=0,litAmbientMix=0,
            mainColorMix=0,shadowOverride=0,useSceneIrradiance=True))

    def test_scene_settings_respect_direction_convention_and_input_ownership(self):
        import copy
        g,_=self.fixture();g[116]=[3,4,0,0];before=copy.deepcopy(g)
        self.assertEqual(subject.decode_scene_settings(g,ambient_row=112)['ambientDirection'],[-.6,.8,0])
        self.assertEqual(subject.decode_scene_settings(g,ambient_row=112,reflect_x=False)['ambientDirection'],[.6,.8,0])
        self.assertEqual(g,before)

    def test_scene_settings_reject_missing_and_invalid_source_fields(self):
        for index in [27,29,110,111,112,116,117,122]:
            g,_=self.fixture();del g[index]
            with self.subTest(missing=index),self.assertRaises(ValueError):
                subject.decode_scene_settings(g,ambient_row=112)
        for index,component,value in [(27,0,17),(29,0,-.1),(110,1,-1),(110,3,17),
            (111,2,1.1),(112,0,65),(116,1,0),(117,0,2),(117,1,17),(117,2,-1),
            (122,1,2),(122,3,float('nan')),(117,1,False)]:
            g,_=self.fixture();g[index][component]=value
            with self.subTest(field=index,component=component),self.assertRaises(ValueError):
                subject.decode_scene_settings(g,ambient_row=112)

    def test_surface_material_colors_follow_explicit_source_rows(self):
        self.assertEqual(subject.decode_surface_lighting(*self.fixture(),ambient_row=113,main_color_row=114),
            dict(schemaVersion=1,ambientColor=[1.2,.9,.8],mainColor=[1,.9,.8]))
        g,o=self.fixture();g[122][1]=0
        self.assertEqual(subject.decode_surface_lighting(g,o,ambient_row=113,main_color_row=114)['mainColor'],[.5,.5,.5])

    def test_zero_global_rim_emits_existing_sparse_material_override(self):
        expected=dict(schemaVersion=2,mode='parameters',typeId='azure.stylized',parameters={'rimAmount':0},textures={})
        for row in ([0,0,0,1],[1,.5,.25,0]):
            self.assertEqual(subject.make_zero_global_rim_override({118:row},rim_row=118),expected)

    def test_nonzero_native_global_rim_requires_its_own_consumer(self):
        with self.assertRaises(ValueError):subject.make_zero_global_rim_override({118:[.1,0,0,1]},rim_row=118)

    def test_global_rim_override_rejects_missing_or_invalid_rows(self):
        for rows in ({},{118:[0,0,0]},{118:[False,0,0,1]},{118:[0,0,float('nan'),1]},{118:[-1,0,0,1]}):
            with self.assertRaises(ValueError):subject.make_zero_global_rim_override(rows,rim_row=118)

    def fixture(self):
        global_rows={27:[1,1,1,1],29:[.2877224684,.2877224684,1,0],110:[1,1.05,.55,.8],111:[0,1,1,1],112:[.8,.9,1.1,1],113:[1.2,.9,.8,1],
            114:[1,.9,.8,1],115:[1,1,1,1],116:[0,1,0,0],117:[.15,1.5,.5,0],121:[.6,.8,0,0],122:[1,1,1,0]}
        object_rows={0:[0,-1,0,0],3:[.5,.5,.5,1.625]}
        return global_rows,object_rows

    def test_surface_uses_captured_object_intensity_and_global_color(self):
        result=subject.decode_lighting(*self.fixture(),ambient_row=113,main_color_row=114)
        self.assertEqual(result['settings']['ambientColor'],[1.2,.9,.8])
        self.assertEqual(result['mainLight']['color'],[1,.9,.8])
        self.assertEqual(result['mainLight']['intensity'],1.625)
        self.assertEqual(result['mainLight']['incomingDirection'],[-.6,.8,0])

    def test_ambient_weight_retains_global_blend_and_gain(self):
        g,o=self.fixture()
        self.assertAlmostEqual(subject.decode_lighting(g,o,ambient_row=112,main_color_row=115)['settings'].get('ambientWeight',-1),.2877224684)
        g[29][0]=.2;g[27][0]=.5;g[122][3]=1
        self.assertAlmostEqual(subject.decode_lighting(g,o,ambient_row=112,main_color_row=115)['settings'].get('ambientWeight',-1),.4)

    def test_ambient_weight_requires_confirmed_global_rows(self):
        g,o=self.fixture();del g[29]
        with self.assertRaises(ValueError):subject.decode_lighting(g,o,ambient_row=112,main_color_row=115)

    def test_explicit_rows_and_blends_keep_light_groups_independent(self):
        g,o=self.fixture();g[122][1]=0;g[122][3]=1;g[111][3]=0
        result=subject.decode_lighting(g,o,ambient_row=112,main_color_row=115)
        self.assertEqual(result['settings']['ambientColor'],[.8,.9,1.1])
        self.assertEqual(result['mainLight']['color'],[.5,.5,.5])
        self.assertEqual(result['mainLight']['intensity'],1)
        self.assertEqual(result['mainLight']['incomingDirection'],[0,1,0])

    def test_rejects_missing_nonfinite_and_invalid_global_ranges(self):
        for row,component,value in [(110,1,float('inf')),(111,2,2),(116,1,0)]:
            g,o=self.fixture();g[row][component]=value
            with self.assertRaises(ValueError):subject.decode_lighting(g,o,ambient_row=113,main_color_row=114)
        g,o=self.fixture();del o[3]
        with self.assertRaises(ValueError):subject.decode_lighting(g,o,ambient_row=113,main_color_row=114)

    def test_projected_hair_joins_confirmed_material_and_global_rows(self):
        result=subject.decode_projected_hair_lighting(*self.fixture(),{4:[0,0,.55,1.1]},
            ambient_row=112,main_color_row=115,shadow_color_row=4)
        self.assertEqual(result,dict(schemaVersion=1,shadowBrightness=.55,shadowSaturation=1.1,
            shadowScale=.55,ambientColor=[.8,.9,1.1],mainColor=[1,1,1]))

    def test_projected_hair_rejects_missing_or_invalid_shadow_inputs(self):
        for material in ({},{4:[0,0,True,1.1]},{4:[0,0,.55,float('nan')]},{4:[0,0,-1,1.1]},{4:[0,0,.55,17]}):
            with self.assertRaises(ValueError):subject.decode_projected_hair_lighting(*self.fixture(),material,
                ambient_row=112,main_color_row=115,shadow_color_row=4)


if __name__=='__main__':unittest.main()
