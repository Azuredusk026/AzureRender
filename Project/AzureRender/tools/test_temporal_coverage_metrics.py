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


if __name__=='__main__':unittest.main()
