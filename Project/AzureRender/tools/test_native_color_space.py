"""Check native float32 color conversion against captured constant values."""
import unittest
from endfield_native.color_space import srgb_to_linear_f32


class NativeColorSpaceTests(unittest.TestCase):
    def test_captured_standard_and_hdr_channels(self):
        self.assertEqual(srgb_to_linear_f32(.7),.4479884207248688)
        self.assertEqual(srgb_to_linear_f32(23.968628,hdr=True),1084.4637451171875)

    def test_white_and_black_are_exact(self):
        self.assertEqual(srgb_to_linear_f32(0),0)
        self.assertEqual(srgb_to_linear_f32(1),1)

    def test_domain_requires_explicit_hdr_and_finite_numbers(self):
        for value in [True,None,'1',-1,float('nan'),float('inf'),1.01]:
            with self.subTest(value=value),self.assertRaises(ValueError):srgb_to_linear_f32(value)

    def test_hdr_overflow_is_rejected(self):
        with self.assertRaises(ValueError):srgb_to_linear_f32(1e38,hdr=True)


if __name__=='__main__':unittest.main()
