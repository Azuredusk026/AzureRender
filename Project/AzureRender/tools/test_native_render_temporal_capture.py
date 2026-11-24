import unittest
import numpy as np
from endfield_native.temporal_capture import decode_motion, history_coordinates, summarize_planes

class TemporalCaptureTests(unittest.TestCase):
    def test_captured_motion_and_reprojection(self):
        # Independent E1579 debugger registers at instruction boundaries 15/24/25.
        fields=[(562,457),(549,561),(516,513)]
        words=np.array([x|(y<<10)|(1<<20) for x,y in fields],dtype=np.uint32)
        expected=np.array([[9.501307795289904e-5,-1.2888541095890105e-4],
            [2.8889746317872778e-5,8.770790009293705e-5],
            [5.990538287647951e-9,7.395726392411817e-11]])
        np.testing.assert_allclose(decode_motion(words),expected,rtol=2e-5,atol=1e-12)
        positions=np.array([[1024.5,720.5],[1317.5,1267.5],[1750.5,480.5]])
        expected_uv=[[.40010032057762146,.5004761219024658],
            [.5146195292472839,.8801206946372986],[.683789074420929,.33368057012557983]]
        np.testing.assert_allclose(history_coordinates(positions,(2560,1440),words),expected_uv,atol=8e-8,rtol=0)

    def test_packing_extremes_and_nonzero_quantized_center(self):
        a=np.array([0,1023|(1023<<10),511|(512<<10)],dtype=np.uint32)
        result=decode_motion(a)
        np.testing.assert_array_equal(result[:2],[[-1,-1],[1,1]])
        self.assertLess(result[2,0],0);self.assertGreater(result[2,1],0)
        np.testing.assert_array_equal(decode_motion(a|np.uint32(0xfff00000)),result)

    def test_invalid_motion_inputs(self):
        for a in (np.array([0.]),np.array([-1]),np.array([True])):
            with self.assertRaises(ValueError):decode_motion(a)
        for size in ((0,1),(1,-1),(float('nan'),1)):
            with self.assertRaises(ValueError):history_coordinates(np.zeros((1,2)),size,np.zeros(1,dtype=np.uint32))
        with self.assertRaises(ValueError):history_coordinates([[float('nan'),0]],(1,1),np.zeros(1,dtype=np.uint32))
        with self.assertRaises(ValueError):history_coordinates(np.zeros((2,2)),(1,1),np.zeros(1,dtype=np.uint32))

    def test_alpha_is_measured_independently_from_history_weight(self):
        mask=np.full((1,2),255,dtype=np.uint8)
        history=np.ones((4,8,4),dtype=np.float16);history[:,:,3]=0
        resolved=history.copy();resolved[:,:,3]=.5
        result=summarize_planes(mask,history,resolved)
        self.assertEqual(result['mask']['nonzeroPixels'],2)
        self.assertEqual(result['historyAlpha']['nonzeroPixels'],0)
        self.assertEqual(result['resolvedAlpha']['nonzeroPixels'],32)
        self.assertTrue(result['nonzeroAlphaObserved'])
        with self.assertRaises(ValueError):summarize_planes(mask,history[:,:4],resolved)
        history[0,0,0]=np.nan
        with self.assertRaises(ValueError):summarize_planes(mask,history,resolved)

if __name__=='__main__':unittest.main()
