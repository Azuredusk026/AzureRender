"""Independent texel and filtering controls for captured irradiance resources."""
import struct
import unittest
import numpy as np
try:
    from endfield_native.irradiance_capture import decode_volume, sample_linear, sample_layer
except ImportError:
    decode_volume=sample_linear=sample_layer=None


class CapturedIrradianceTests(unittest.TestCase):
    def test_unsigned_float_channels_and_subnormal(self):
        self.assertIsNotNone(decode_volume)
        # Exponent 15 encodes one; green adds half, blue adds a quarter.
        word=960 | (992<<11) | (488<<22)
        v=decode_volume(struct.pack('<I',word),'R11G11B10_FLOAT',(1,1,1))
        np.testing.assert_array_equal(v[0,0,0],[1,1.5,1.25])
        sub=decode_volume(struct.pack('<I',1),'R11G11B10_FLOAT',(1,1,1))
        self.assertEqual(sub[0,0,0,0],2**-20)
        with self.assertRaises(ValueError):decode_volume(struct.pack('<I',1984),'R11G11B10_FLOAT',(1,1,1))

    def test_filtering_uses_texel_centers_and_all_eight_corners(self):
        self.assertIsNotNone(sample_linear)
        v=np.arange(8,dtype=float).reshape(2,2,2,1)
        self.assertEqual(sample_linear(v,[.25,.25,.25],'clamp')[0],0)
        self.assertEqual(sample_linear(v,[.5,.5,.5],'clamp')[0],3.5)
        self.assertEqual(sample_linear(v,[.75,.25,.25],'clamp')[0],1)
        self.assertEqual(sample_linear(v,[-1,0,0],'clamp')[0],0)
        self.assertEqual(sample_linear(v,[0,.25,.25],'wrap')[0],.5)
        np.testing.assert_array_equal(sample_linear(v,[-.25,.25,.25],'wrap'),[1])

    def test_direction_groups_do_not_bleed_at_vertical_edges(self):
        self.assertIsNotNone(sample_layer)
        amplitude=np.ones((2,2,2,3))*2
        direction=np.empty((2,6,2,4))
        for c in range(3):direction[:,c*2:(c+1)*2,:,:]=[c/2,0,1,.25*c]
        for position in ([0,-.5,0],[0,.5,0],[0,1.49,0]):
            got=sample_layer(position,.5,amplitude,direction)
            np.testing.assert_array_equal(got['amplitude'],[2]*3)
            np.testing.assert_allclose(got['directionalRgba'],[[0,0,1,0],[.5,0,1,.25],[1,0,1,.5]],atol=1e-15)

    def test_validation_rejects_wrong_layout_and_nonfinite_coordinates(self):
        self.assertIsNotNone(decode_volume)
        for dims in ((0,2,2),(True,1,1),(1,1),(1.5,1,1)):
            with self.assertRaises(ValueError):decode_volume(b'1234','RGBA8_UNORM',dims)
        with self.assertRaises(ValueError):decode_volume(b'12','RGBA8_UNORM',(1,1,1))
        with self.assertRaises(ValueError):decode_volume(b'1234','RGBA8_SRGB',(1,1,1))
        with self.assertRaises(ValueError):sample_linear(np.zeros((1,1,1,3)),[float('nan'),0,0],'wrap')
        with self.assertRaises(ValueError):sample_layer([0,0,0],0,np.zeros((1,1,1,3)),np.zeros((1,3,1,4)))
        with self.assertRaises(ValueError):sample_layer([0,0,0],1,np.zeros((1,1,1,3)),np.zeros((1,4,1,4)))


if __name__=='__main__':unittest.main()
