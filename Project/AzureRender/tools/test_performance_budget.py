"""Formal acceptance boundaries shared by playable measurements."""
import copy
import math
import unittest

from performance_budget import working_set_growth, validate_round


def passing_round():
    return dict(gpu={'p95': 16.6}, cpu={'p95': 8}, physics={'p95': 2},
        samples=1800, warmup=300, peakWorkingSetBytes=2*1024**3,
        allocator={'deviceLocalPeakBytes': 1.5*1024**3},
        initialDevice={'temperature': 55}, workingSetSamplesBytes=[100]*9+[105],
        stableWorkingSetGrowth=.05)


class FormalBudgetTests(unittest.TestCase):
    def test_boundary_and_editor_cpu(self):
        self.assertEqual(validate_round(passing_round(), editor=False), [])
        result=passing_round(); result['cpu']['p95']=10
        self.assertEqual(validate_round(result, editor=True), [])
        self.assertTrue(validate_round(result, editor=False))

    def test_pressure_load_has_same_gpu_and_memory_budget(self):
        for field, value in [('gpu',16.7),('cpu',8.001),('physics',2.001)]:
            result=passing_round(); result[field]['p95']=value
            self.assertTrue(validate_round(result, editor=False), field)
        for key, value in [('peakWorkingSetBytes',2*1024**3+1),('samples',1799),('warmup',299)]:
            result=passing_round(); result[key]=value
            self.assertTrue(validate_round(result, editor=False), key)
        result=passing_round(); result['allocator']['deviceLocalPeakBytes']+=1
        self.assertTrue(validate_round(result, editor=False))

    def test_growth_uses_original_p5_formula(self):
        self.assertAlmostEqual(working_set_growth([900]*5+[100,101,105,99,103]), .05)
        result=passing_round(); result['workingSetSamplesBytes'][-1]=106
        self.assertTrue(validate_round(result, editor=False))

    def test_invalid_memory_is_not_a_passing_measurement(self):
        for samples in ([], [100]*9, [100]*9+[0], [100]*9+[-1], [100]*9+[math.nan]):
            with self.assertRaises(ValueError): working_set_growth(samples)
            result=passing_round(); result['workingSetSamplesBytes']=samples
            self.assertTrue(validate_round(result, editor=False))
        result=passing_round(); result['peakWorkingSetBytes']=0
        self.assertTrue(validate_round(result, editor=False))

    def test_nan_infinity_negative_and_missing_metrics(self):
        for field in ('gpu','cpu','physics'):
            for value in (math.nan,math.inf,-.1):
                result=passing_round(); result[field]['p95']=value
                self.assertTrue(validate_round(result, editor=False))
        for key in passing_round():
            result=passing_round(); del result[key]
            self.assertTrue(validate_round(result, editor=False), key)

    def test_growth_claim_must_match_observed_samples(self):
        result=passing_round(); result['stableWorkingSetGrowth']=0
        self.assertTrue(validate_round(result, editor=False))

    def test_thermal_gate(self):
        result=passing_round(); result['initialDevice']['temperature']=56
        self.assertTrue(validate_round(result, editor=False))

    def test_validation_preserves_raw_evidence(self):
        result=passing_round(); before=copy.deepcopy(result)
        validate_round(result, editor=False)
        self.assertEqual(result,before)


if __name__=='__main__': unittest.main()
