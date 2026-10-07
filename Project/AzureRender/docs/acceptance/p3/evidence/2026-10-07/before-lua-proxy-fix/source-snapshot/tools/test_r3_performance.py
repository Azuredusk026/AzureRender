import unittest

from run_r3_performance import evaluate_budget


class BudgetTests(unittest.TestCase):
    def evaluate(self, cpu=0.4, serial_cpu=0.4, gpu=1.0, serial_gpu=1.0, small=False):
        return evaluate_budget({"parallel_gpu": {"cpuRecordingMs": cpu, "gpuMs": gpu},
                                "serial_gpu": {"cpuRecordingMs": serial_cpu, "gpuMs": serial_gpu}}, small)

    def test_multi_boundaries(self):
        self.assertTrue(self.evaluate(cpu=0.125, serial_cpu=0.025, gpu=1.05)["passed"])
        for values in ({"cpu": 0.5}, {"cpu": 0.401, "serial_cpu": 0.3}, {"gpu": 1.050001}):
            self.assertFalse(self.evaluate(**values)["passed"])

    def test_small_boundaries(self):
        self.assertTrue(self.evaluate(cpu=0.499, gpu=2.999, small=True)["passed"])
        self.assertFalse(self.evaluate(cpu=0.5, small=True)["passed"])
        self.assertFalse(self.evaluate(gpu=3.0, small=True)["passed"])

    def test_invalid_samples(self):
        for value in (float("nan"), float("inf"), -0.1):
            for key in ("cpu", "gpu", "serial_cpu", "serial_gpu"):
                with self.assertRaises(ValueError):
                    self.evaluate(**{key: value})
        with self.assertRaises(ValueError):
            self.evaluate(serial_gpu=0)


if __name__ == "__main__":
    unittest.main()
