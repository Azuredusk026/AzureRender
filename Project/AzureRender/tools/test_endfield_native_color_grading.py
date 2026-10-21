"""Checks for the source-bound full native grading candidate generator."""
import importlib.util
from pathlib import Path
import unittest
import numpy as np
import tempfile
import json
import struct


class NativeColorGradingTests(unittest.TestCase):
    def module(self):
        path = Path(__file__).parent / "endfield_native/color_grading.py"
        self.assertTrue(path.is_file(), "Full native matrix and grading-chain candidate generator is required")
        spec=importlib.util.spec_from_file_location("native_color_grading",path)
        module=importlib.util.module_from_spec(spec);spec.loader.exec_module(module)
        return module

    def test_log_decode_and_complete_chain(self):
        m=self.module();p=m.neutral_candidate_parameters()
        decoded=m.decode_logc3(np.array([0.,.39100704,1.]))
        self.assertAlmostEqual(decoded[1],.18,places=6)
        self.assertTrue(decoded[0]<0 and decoded[2]>50)
        points=np.array([[0,0,0],[.18,.18,.18],[2,.1,.3],[10,1,.01]])
        result,trace=m.evaluate_native_chain(points,p,return_trace=True)
        self.assertEqual(result.shape,points.shape)
        self.assertTrue(np.isfinite(result).all())
        self.assertTrue(((result>=0)&(result<=1)).all())
        required={"white_balance","aces_ap1","contrast_filter","split_tone","channel_mixer",
                  "shadows_midtones_highlights","lift_gamma_gain","hue_saturation","master_rgb_curves","aces_modified","output_matrix","highlight_normalization"}
        self.assertEqual(set(trace),required)
        self.assertFalse(np.allclose(result[2],m.modified_aces_curve(points[2])),"Native output includes matrix transforms and the complete chain")

    def test_authored_controls_and_validation(self):
        m=self.module();p=m.neutral_candidate_parameters()
        points=np.array([[.2,.1,.05],[.5,.8,.2]])
        baseline=m.evaluate_native_chain(points,p)
        p["_ChannelMixerRed"]=[0,1,0]
        self.assertFalse(np.allclose(baseline,m.evaluate_native_chain(points,p)))
        p=m.neutral_candidate_parameters();p["_HueSatCon"]=[0,0,1]
        result=m.evaluate_native_chain(points,p)
        self.assertLess(np.max(np.abs(result[:,0]-result[:,1])),.002)
        p=m.neutral_candidate_parameters();p["_Gamma"]=[float("nan"),1,1]
        with self.assertRaises(ValueError):m.evaluate_native_chain(points,p)
        with self.assertRaises(ValueError):m.evaluate_native_chain(points,{})
        with self.assertRaises(ValueError):m.build_candidate_lattice(65)

    def test_portable_binary_and_candidate_receipt(self):
        m=self.module();pixels,receipt=m.build_candidate_lattice(4)
        self.assertEqual(pixels.shape,(4,16,4))
        self.assertEqual(receipt["evidenceStatus"],"candidate")
        self.assertTrue(receipt["remainingUnknowns"])
        with tempfile.TemporaryDirectory() as root:
            path=Path(root)/"candidate.azlut"
            m.write_azlut(path,pixels,"unit-test-source-sha256")
            raw=path.read_bytes();self.assertEqual(raw[:8],b"AZLUT001")
            length=struct.unpack("<I",raw[8:12])[0];header=json.loads(raw[12:12+length])
            self.assertEqual(header["inputDomain"],"logc3-full-log")
            self.assertEqual(header["source"]["evidence"],1)
            recovered=np.frombuffer(raw[12+length:],dtype="<f4").reshape(pixels.shape)
            np.testing.assert_array_equal(recovered,pixels)


if __name__ == "__main__":
    unittest.main()
