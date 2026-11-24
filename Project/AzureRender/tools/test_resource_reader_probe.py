import json
from pathlib import Path
import subprocess
import sys
import tempfile
import unittest


class ReaderProbeTests(unittest.TestCase):
    def run_probe(self, setup):
        with tempfile.TemporaryDirectory() as tmp:
            root = Path(tmp)
            setup(root)
            output = root / 'result.json'
            process = subprocess.run([sys.executable, str(Path(__file__).with_name('probe_resource_reader.py')),
                '--bin-dir', str(root), '--game-root', str(root), '--output', str(output)], capture_output=True)
            self.assertTrue(output.exists(), process.stderr.decode(errors='replace'))
            return process.returncode, json.loads(output.read_text(encoding='utf-8'))

    def test_missing_dll_is_diagnostic_not_success(self):
        code, report = self.run_probe(lambda root: None)
        self.assertNotEqual(code, 0)
        self.assertEqual(report['error']['code'], 'missing_file')
        self.assertFalse(report['resource_read_verified'])

    def test_unsupported_framework_is_rejected_before_launch(self):
        def setup(root):
            (root / 'Ruri.RipperHook.dll').write_bytes(b'not executed')
            (root / 'Ruri.RipperHook.CLI.runtimeconfig.json').write_text(json.dumps({'runtimeOptions': {'tfm': 'net8.0'}}))
        code, report = self.run_probe(setup)
        self.assertNotEqual(code, 0)
        self.assertEqual(report['error']['code'], 'unsupported_framework')

    def test_missing_declared_dependency_reports_exact_file(self):
        def setup(root):
            (root / 'Ruri.RipperHook.dll').write_bytes(b'not executed')
            (root / 'Ruri.RipperHook.CLI.runtimeconfig.json').write_text(json.dumps({'runtimeOptions': {'tfm': 'net10.0'}}))
            (root / 'Ruri.RipperHook.CLI.deps.json').write_text(json.dumps({'targets': {'net10.0': {'package/1': {'runtime': {'required.dll': {}}}}}}))
        code, report = self.run_probe(setup)
        self.assertNotEqual(code, 0)
        self.assertEqual(report['error']['code'], 'missing_dependency')
        self.assertIn('required.dll', report['error']['message'])


if __name__ == '__main__':
    unittest.main()
