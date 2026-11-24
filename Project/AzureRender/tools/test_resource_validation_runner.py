import importlib.util
import json
from pathlib import Path
import sys
import tempfile
import unittest


class ValidationRunnerTests(unittest.TestCase):
    def setUp(self):
        path = Path(__file__).with_name('run_resource_validation.py')
        self.assertTrue(path.exists(), 'Validation runner is missing')
        spec = importlib.util.spec_from_file_location('runner', path)
        self.runner = importlib.util.module_from_spec(spec)
        spec.loader.exec_module(self.runner)

    def test_nonzero_exit_preserves_failure_logs_and_continues(self):
        with tempfile.TemporaryDirectory() as temp:
            result = self.runner.execute({'steps': [
                {'id': 'fail', 'argv': [sys.executable, '-c', 'print("evidence");raise SystemExit(7)']},
                {'id': 'next', 'argv': [sys.executable, '-c', 'print("next")']}]}, Path(temp), {})
            self.assertFalse(result['passed'])
            self.assertEqual(result['steps'][0]['exitCode'], 7)
            self.assertEqual(result['steps'][1]['status'], 'passed')
            self.assertIn('evidence', (Path(temp) / 'fail/stdout.log').read_text())

    def test_zero_exit_without_artifact_fails(self):
        with tempfile.TemporaryDirectory() as temp:
            result = self.runner.execute({'steps': [{'id': 'empty', 'argv': [sys.executable, '-c', 'pass'], 'requiredFiles': ['frame.png']}]}, Path(temp), {})
            self.assertFalse(result['passed'])
            self.assertIn('frame.png', result['steps'][0]['missingFiles'])

    def test_timeout_and_partial_output_are_recorded(self):
        with tempfile.TemporaryDirectory() as temp:
            result = self.runner.execute({'steps': [{'id': 'slow', 'argv': [sys.executable, '-u', '-c', 'import time;print("start");time.sleep(5)'], 'timeout': .2}]}, Path(temp), {})
            self.assertEqual(result['steps'][0]['status'], 'timeout')
            self.assertIn('start', (Path(temp) / 'slow/stdout.log').read_text())

    def test_success_requires_real_output_and_log_contract(self):
        with tempfile.TemporaryDirectory() as temp:
            result = self.runner.execute({'steps': [{'id': 'ok', 'argv': [sys.executable, '-c', 'from pathlib import Path;Path("report.json").write_text("{}");print("cleanup")'], 'requiredFiles': ['report.json'], 'requiredLog': ['cleanup']}]}, Path(temp), {})
            self.assertTrue(result['passed'])
            self.assertEqual(len(result['steps'][0]['artifacts'][0]['sha256']), 64)

    def test_unsafe_id_and_existing_output_rejected(self):
        with tempfile.TemporaryDirectory() as temp:
            with self.assertRaises(ValueError):
                self.runner.execute({'steps': [{'id': '../escape', 'argv': [sys.executable]}]}, Path(temp), {})
            (Path(temp) / 'same').mkdir()
            with self.assertRaises(FileExistsError):
                self.runner.execute({'steps': [{'id': 'same', 'argv': [sys.executable]}]}, Path(temp), {})


if __name__ == '__main__':
    unittest.main()
