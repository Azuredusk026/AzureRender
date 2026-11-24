"""Verify raw captured texels before temporal and grading comparisons."""
import math
import struct
import unittest
from endfield_native.capture_pixels import read_pixel


class CapturePixelTests(unittest.TestCase):
    def test_unsigned_float_mantissas_and_subnormal_values(self):
        word=(15<<6)|((16<<6)<<11)|((17<<5)<<22)
        self.assertEqual(read_pixel(struct.pack('<I',word),pixel_format='R11G11B10_FLOAT',width=1,height=1,x=0,y=0,flip_y=False),(1,2,4,1))
        self.assertEqual(read_pixel(struct.pack('<I',1),pixel_format='R11G11B10_FLOAT',width=1,height=1,x=0,y=0,flip_y=False),(2**-20,0,0,1))

    def test_explicit_origin_and_unorm_are_independent_of_format(self):
        raw=bytes([0,128,255,64,255,0,64,255])
        args=dict(pixel_format='R8G8B8A8_UNORM',width=1,height=2,x=0,y=0)
        self.assertEqual(read_pixel(raw,**args,flip_y=False),(0,128/255,1,64/255))
        self.assertEqual(read_pixel(raw,**args,flip_y=True),(1,0,64/255,1))
        half=struct.pack('<8e',-2,.5,4,1,8,16,.25,0)
        self.assertEqual(read_pixel(half,**{**args,'pixel_format':'R16G16B16A16_FLOAT'},flip_y=True),(8,16,.25,0))

    def test_rejects_nonfinite_channels_unknown_layout_and_invalid_ranges(self):
        args=dict(pixel_format='R11G11B10_FLOAT',width=1,height=1,x=0,y=0,flip_y=False)
        cases=[(b'',{}),('text',{}),(bytes(4),{'x':1}),(bytes(4),{'x':.5}),(bytes(4),{'y':-1}),(bytes(4),{'width':True}),
               (bytes(4),{'width':0}),(bytes(4),{'height':-1}),
               (bytes(4),{'flip_y':1}),(bytes(4),{'pixel_format':'unknown'}),
               (struct.pack('<I',31<<6),{}),
               (struct.pack('<I',((31<<5)|1)<<22),{}),
               (struct.pack('<4e',0,math.inf,0,1),{'pixel_format':'R16G16B16A16_FLOAT'})]
        for raw,update in cases:
            with self.assertRaises(ValueError):read_pixel(raw,**{**args,**update})


if __name__=='__main__':unittest.main()
