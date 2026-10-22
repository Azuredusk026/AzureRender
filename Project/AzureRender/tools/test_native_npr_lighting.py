"""Decode explicit NPR globals and object blends from a confirmed draw."""
import unittest
from endfield_native import npr_lighting as subject


class NprLightingTests(unittest.TestCase):
    def fixture(self):
        global_rows={110:[1,1.05,.55,.8],111:[0,1,1,1],112:[.8,.9,1.1,1],113:[1.2,.9,.8,1],
            114:[1,.9,.8,1],115:[1,1,1,1],116:[0,1,0,0],117:[.15,1.5,.5,0],121:[.6,.8,0,0],122:[1,1,1,0]}
        object_rows={0:[0,-1,0,0],3:[.5,.5,.5,1.625]}
        return global_rows,object_rows

    def test_surface_uses_captured_object_intensity_and_global_color(self):
        result=subject.decode_lighting(*self.fixture(),ambient_row=113,main_color_row=114)
        self.assertEqual(result['settings']['ambientColor'],[1.2,.9,.8])
        self.assertEqual(result['mainLight']['color'],[1,.9,.8])
        self.assertEqual(result['mainLight']['intensity'],1.625)
        self.assertEqual(result['mainLight']['incomingDirection'],[-.6,.8,0])

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


if __name__=='__main__':unittest.main()
