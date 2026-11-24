"""Reference-correlated detail rejects erased structure and unrelated noise."""
import unittest
import numpy as np
from unittest.mock import patch
import test_temporal_alpha_coverage_gpu as coverage


class DetailMetrics(unittest.TestCase):
    def test_signal_gain_and_constant_input(self):
        x=np.arange(32)*2*np.pi/32
        reference=np.broadcast_to((.5+.2*np.sin(x))[None,None,:,None],(80,4,32,3)).copy()
        with patch.object(coverage.motion,'local',side_effect=lambda value:value):
            for gain in (0,.5,1,1.2):
                actual=.5+(reference-.5)*gain
                np.testing.assert_allclose(coverage.coherent_detail(actual,reference),gain,atol=1e-12)

    def test_uncorrelated_contrast_does_not_replace_structure(self):
        x=np.arange(32)*2*np.pi/32
        reference=np.broadcast_to((.5+.2*np.sin(x))[None,None,:,None],(80,4,32,3)).copy()
        unrelated=np.broadcast_to((.5+.2*np.cos(x))[None,None,:,None],reference.shape).copy()
        with patch.object(coverage.motion,'local',side_effect=lambda value:value):
            np.testing.assert_allclose(coverage.coherent_detail(unrelated,reference),0,atol=1e-12)

    def test_envelope_does_not_hide_erased_fine_structure(self):
        x=np.arange(64,dtype=float)
        envelope=.2+.3*np.exp(-((x-31.5)/15)**2)
        fine=.01*np.cos(x*np.pi*.5)
        reference=np.broadcast_to((envelope+fine)[None,None,:,None],(4,8,64,3)).copy()
        erased=np.broadcast_to(envelope[None,None,:,None],reference.shape).copy()
        self.assertGreater(min(coverage.correlated_detail(erased,reference)),.95)
        self.assertLess(max(coverage.correlated_detail(erased,reference,highpass=True)),.2)

    def test_high_frequency_gain_and_input_ownership(self):
        x=np.arange(32,dtype=float)*2*np.pi/8
        reference=np.broadcast_to((.5+.2*np.sin(x))[None,None,:,None],(4,8,32,3)).copy()
        frozen=reference.copy()
        for gain in (0,.5,1,1.2):
            candidate=.5+(reference-.5)*gain
            np.testing.assert_allclose(coverage.correlated_detail(candidate,reference,highpass=True),gain,atol=1e-12)
        np.testing.assert_array_equal(reference,frozen)

    def test_invalid_and_empty_signals_are_rejected(self):
        reference=np.ones((4,8,32,3),dtype=float)
        for candidate in (reference[:2],reference[0],reference.astype('uint8'),reference*np.nan):
            with self.assertRaises(ValueError):coverage.correlated_detail(candidate,reference)
        with self.assertRaises(ValueError):coverage.correlated_detail(reference,reference,indices=[])
        with self.assertRaises(ValueError):coverage.correlated_detail(reference,reference)


if __name__=='__main__':unittest.main()
