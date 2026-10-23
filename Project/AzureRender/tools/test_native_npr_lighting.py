"""Decode explicit NPR globals and object blends from a confirmed draw."""
import unittest
from endfield_native import npr_lighting as subject


class NprLightingTests(unittest.TestCase):
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
