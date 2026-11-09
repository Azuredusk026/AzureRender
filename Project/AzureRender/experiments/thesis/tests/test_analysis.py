import importlib.util
import math
from pathlib import Path
import unittest

class AnalysisTests(unittest.TestCase):
    def module(self):
        path=Path(__file__).resolve().parents[1]/'tools/analysis.py'
        self.assertTrue(path.is_file(), 'run-level analysis must exist')
        spec=importlib.util.spec_from_file_location('analysis',path)
        module=importlib.util.module_from_spec(spec); spec.loader.exec_module(module)
        return module

    def test_variance_and_interpolated_percentile_use_known_samples(self):
        result=self.module().describe([1,2,3])
        self.assertEqual(result['meanMs'],2)
        self.assertEqual(result['varianceMs2'],1)
        self.assertAlmostEqual(result['p95Ms'],2.9)

    def test_invalid_samples_are_rejected_without_silent_dropping(self):
        module=self.module()
        for samples in [[1], [1,math.nan], [1,math.inf], [1,-1], [True,2]]:
            with self.subTest(samples=samples), self.assertRaises(ValueError):
                module.describe(samples)

    def test_attachment_lifetime_keeps_normal_store_and_contour_neighbors(self):
        model=self.module().traffic(1920,1080,'subpasses')
        self.assertEqual(model['logicalBytes'],165888000)
        self.assertEqual(model['localReuseScenarioBytes'],132710400)
        self.assertEqual(model['normalStoreBytes'],16588800)
        self.assertEqual(model['contourNormalReadBytes'],82944000)
        self.assertFalse(model['hardwareMeasured'])

    def test_unknown_path_and_invalid_extent_are_rejected(self):
        module=self.module()
        for args in [(0,1,'traditional'),(1,1,'dynamic'),(True,1,'drlr')]:
            with self.assertRaises(ValueError): module.traffic(*args)

    def test_declared_counts_do_not_substitute_for_executed_path(self):
        module=self.module()
        run=dict(path='drlr',warmup=2,gpuMs=[1,2],cpuRecordingMs=[0.1,0.2],
                 geometryDraws=4,toonDraws=4,contourDraws=4,nextSubpassCalls=0,beginRenderingCalls=0)
        with self.assertRaises(ValueError): module.summarize(run)

if __name__=='__main__': unittest.main()
