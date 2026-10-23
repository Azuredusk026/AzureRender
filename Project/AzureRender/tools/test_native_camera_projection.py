"""Validate explicit captured projection columns and public camera offsets."""
import math
import unittest
from endfield_native import camera_projection as subject


class CameraProjectionTests(unittest.TestCase):
    def perspective(self):
        return [1.784,0,0,0,0,-3.1716,0,0,-1/5120,-1/4320,.00001,-1,0,0,.100001,0]

    def test_perspective_offset_uses_clip_w_and_raster_axis_convention(self):
        result=subject.decode_raster_offset(self.perspective(),projection_kind='perspective')
        self.assertEqual(result,dict(projectionOffsetX=1/5120,projectionOffsetY=1/4320))
        self.assertAlmostEqual(result['projectionOffsetX']*2560/2,.25)
        self.assertAlmostEqual(result['projectionOffsetY']*1440/2,1/6)

    def test_orthographic_offset_uses_translation(self):
        matrix=[1,0,0,0,0,-1,0,0,0,0,-.2,0,.25,-.5,0,1]
        self.assertEqual(subject.decode_raster_offset(matrix,projection_kind='orthographic'),dict(projectionOffsetX=.25,projectionOffsetY=-.5))

    def test_invalid_matrices_and_unconfirmed_axis_layout_are_rejected(self):
        cases=[self.perspective()[:-1]]
        for i,value in ((0,-1),(5,3),(8,3),(9,float('nan')),(11,1),(15,1),(0,True)):
            matrix=self.perspective();matrix[i]=value;cases.append(matrix)
        for matrix in cases:
            with self.assertRaises(ValueError):subject.decode_raster_offset(matrix,projection_kind='perspective')
        with self.assertRaises(ValueError):subject.decode_raster_offset(self.perspective(),projection_kind='unknown')


if __name__=='__main__':unittest.main()
