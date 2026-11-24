import copy
import unittest

from run_blackhole_optimization import evaluate_budget, validate_report


class BudgetTests(unittest.TestCase):
    def setUp(self):
        self.report = {"samples": 300, "width": 1280, "height": 720,
                       "totalAverageMs": 19.0, "totalP95Ms": 21.0,
                       "submission": {"frames": 300, "recordingMilliseconds": 60.0}}

    def test_strict_budget_and_median(self):
        self.assertTrue(evaluate_budget([19.0, 19.9, 23.0])["passed"])
        self.assertFalse(evaluate_budget([19.0, 20.0, 21.0])["passed"])

    def test_requires_three_complete_runs(self):
        for values in ([], [19.0], [19.0, 19.0], [19.0] * 4):
            with self.assertRaises(ValueError):
                evaluate_budget(values)
        validate_report(self.report)
        for key, value in (("samples", 299), ("width", 904), ("height", 508)):
            report = copy.deepcopy(self.report)
            report[key] = value
            with self.assertRaises(ValueError):
                validate_report(report)
        self.report["submission"]["frames"] = 299
        with self.assertRaises(ValueError):
            validate_report(self.report)

    def test_invalid_timings(self):
        for value in (-1.0, float("nan"), float("inf")):
            with self.assertRaises(ValueError):
                evaluate_budget([19.0, value, 19.0])
            self.report["totalAverageMs"] = value
            with self.assertRaises(ValueError):
                validate_report(self.report)


if __name__ == "__main__":
    unittest.main()
