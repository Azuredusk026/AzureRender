import copy
import unittest

from run_r4_acceptance import validate_report, validate_lifecycle, validate_capture_hashes


class AcceptanceTests(unittest.TestCase):
    def setUp(self):
        self.report = {"samples": 300, "totalAverageMs": 4.0, "totalP95Ms": 5.0,
                       "totalP99Ms": 6.0, "submission": {"frames": 300,
                       "instances": 9600, "workerRecordedPasses": 1200,
                       "indirectDrawCalls": 9900, "recordingMilliseconds": 140.0}}

    def test_real_paths(self):
        validate_report(self.report, 300, "parallel_gpu", 32)
        for key, value in (("workerRecordedPasses", 0), ("indirectDrawCalls", 0), ("instances", 32)):
            bad = copy.deepcopy(self.report)
            bad["submission"][key] = value
            with self.assertRaises(ValueError):
                validate_report(bad, 300, "parallel_gpu", 32)
        with self.assertRaises(ValueError):
            validate_report(self.report, 300, "serial_cpu", 32)
        self.report["submission"].update(workerRecordedPasses=0, indirectDrawCalls=0)
        validate_report(self.report, 300, "serial_cpu", 32)

    def test_incomplete_and_invalid_timing(self):
        self.report["samples"] = 299
        with self.assertRaises(ValueError):
            validate_report(self.report, 300, "parallel_gpu", 32)
        self.report["samples"] = 300
        for value in (-1, float("nan"), float("inf")):
            self.report["totalAverageMs"] = value
            with self.assertRaises(ValueError):
                validate_report(self.report, 300, "parallel_gpu", 32)

    def test_lifecycle_requires_observed_events(self):
        log = ("Swapchain recreated\nBlackhole history reset after recreate\n" * 6
               + "Allocator after unload: buffers=0 images=0\nScreenshot saved: image.png\n")
        validate_lifecycle(log, 6, True)
        for bad in (log.replace("Swapchain recreated", ""),
                    log.replace("Blackhole history reset after recreate", ""),
                    log.replace("buffers=0", "buffers=1"), log + "VUID-error"):
            with self.assertRaises(ValueError):
                validate_lifecycle(bad, 6, True)

    def test_complex_capture_determinism(self):
        validate_capture_hashes(["a", "b"], ["a", "b"])
        for candidate in (["a"], ["a", "c"], ["b", "a"]):
            with self.assertRaises(ValueError):
                validate_capture_hashes(["a", "b"], candidate)


if __name__ == "__main__":
    unittest.main()
