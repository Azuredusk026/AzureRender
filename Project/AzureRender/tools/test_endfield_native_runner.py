"""Exercise reader selection and argument forwarding in an isolated process."""
import json
from pathlib import Path
import subprocess
import sys
import tempfile
import unittest


class ReaderRunnerTests(unittest.TestCase):
    def run_reader(self, mode=None):
        with tempfile.TemporaryDirectory(prefix='native reader ') as temporary:
            root = Path(temporary)
            source = root / 'source'
            package = source / 'extractor_core'
            package.mkdir(parents=True)
            (package / '__init__.py').write_text('')
            (package / 'vfs.py').write_text(
                'def parse_blc(data, **expected):\n'
                '    return {"data": data.decode(), "expected": expected}\n')
            dependencies = root / 'dependencies'
            dependencies.mkdir()
            script = root / 'entry point.py'
            script.write_text(
                'import json, sys\nfrom extractor_core import vfs\n'
                'print(json.dumps({"arguments":sys.argv[1:],'
                '"parser":vfs.parse_blc.__name__,'
                '"result":vfs.parse_blc(b"revision5", expected_chunk_count=2)'
                ' if vfs.parse_blc.__name__ == "parse_blc" else None}))\n')
            command = [sys.executable, str(Path(__file__).parent /
                       'endfield_native/run_upstream.py'),
                       '--source-root', str(source),
                       '--dependency-root', str(dependencies),
                       '--script', str(script)]
            if mode is not None:
                command += ['--catalog-mode', mode]
            return subprocess.run(command + ['--', '--scene', 'with spaces'],
                                  capture_output=True, text=True, timeout=30)

    def test_upstream_parser_receives_current_catalog_unchanged(self):
        result = self.run_reader('upstream')
        self.assertEqual(result.returncode, 0, result.stderr)
        payload = json.loads(result.stdout)
        self.assertEqual(payload['result'], {
            'data': 'revision5', 'expected': {'expected_chunk_count': 2}})
        self.assertEqual(payload['arguments'], ['--scene', 'with spaces'])

    def test_default_preserves_legacy_adapter(self):
        result = self.run_reader()
        self.assertEqual(result.returncode, 0, result.stderr)
        self.assertEqual(json.loads(result.stdout)['parser'], 'parse')

    def test_explicit_legacy_adapter_remains_available(self):
        result = self.run_reader('legacy')
        self.assertEqual(result.returncode, 0, result.stderr)
        self.assertEqual(json.loads(result.stdout)['parser'], 'parse')

    def test_unknown_catalog_mode_is_rejected(self):
        result = self.run_reader('guess')
        self.assertNotEqual(result.returncode, 0)
        self.assertIn('invalid choice', result.stderr)


if __name__ == '__main__':
    unittest.main()
