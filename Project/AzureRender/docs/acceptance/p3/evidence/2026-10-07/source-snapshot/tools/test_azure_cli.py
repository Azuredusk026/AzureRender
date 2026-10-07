"""Verify developer command routing, portable paths and controlled builds."""
import importlib.util
import json
import os
from pathlib import Path
import subprocess
import sys
import tempfile
import unittest

CLI = Path(__file__).with_name('azure.py')
spec = importlib.util.spec_from_file_location('azure_cli', CLI)
azure = importlib.util.module_from_spec(spec)
spec.loader.exec_module(azure)


class CliTests(unittest.TestCase):
    def test_preset_is_configuration_source(self):
        with tempfile.TemporaryDirectory(prefix='azure cli ') as temporary:
            source = Path(temporary)
            source.joinpath('CMakePresets.json').write_text(json.dumps({'configurePresets': [{
                'name': 'probe', 'binaryDir': '${sourceDir}/build with space',
                'cacheVariables': {'CMAKE_BUILD_TYPE': 'Debug'}}]}))
            build, config = azure.preset(source, 'probe')
            self.assertEqual(build, source / 'build with space')
            self.assertEqual(config, 'Debug')
            with self.assertRaises(ValueError):
                azure.preset(source, 'missing')

    def test_build_lock_and_failure_release(self):
        with tempfile.TemporaryDirectory(prefix='azure lock ') as temporary:
            directory = Path(temporary)
            with azure.build_lock(directory):
                with self.assertRaises(RuntimeError):
                    with azure.build_lock(directory):
                        self.fail('Concurrent build acquired lock')
            with self.assertRaises(ValueError):
                with azure.build_lock(directory):
                    raise ValueError('Build failed')
            with azure.build_lock(directory):
                pass

    def test_process_status_and_space_arguments(self):
        with tempfile.TemporaryDirectory(prefix='azure arguments ') as temporary:
            path = Path(temporary) / 'argument file.txt'
            status = azure.execute([sys.executable, '-c', 'import pathlib,sys;pathlib.Path(sys.argv[1]).write_text(sys.argv[2]);sys.exit(7)', str(path), 'value with spaces'], Path(temporary))
            self.assertEqual(status, 7)
            self.assertEqual(path.read_text(), 'value with spaces')

    def test_help_describe_and_unknown_target(self):
        result = subprocess.run([sys.executable, str(CLI), 'describe', '--json'], capture_output=True, text=True)
        self.assertEqual(result.returncode, 0, result.stderr)
        data = json.loads(result.stdout)
        self.assertIn('validate', data['commands'])
        self.assertIn('msvc-release', data['presets'])
        bad = subprocess.run([sys.executable, str(CLI), 'build', '--target', 'InventedTarget', '--json'], capture_output=True, text=True)
        self.assertNotEqual(bad.returncode, 0)
        self.assertIn('Unknown build target', bad.stderr)
        help_result = subprocess.run([sys.executable, str(CLI), '--help'], capture_output=True, text=True)
        self.assertEqual(help_result.returncode, 0)

    def test_child_environment_and_argument_forwarding(self):
        build, config = azure.preset(CLI.parent.parent, 'msvc-debug')
        command = azure.runtime_command(build, config, 'AzurePlayer', ['--project', 'project with spaces/project.azureproject'])
        self.assertEqual(command[-2:], ['--project', 'project with spaces/project.azureproject'])
        with self.assertRaises(ValueError):
            azure.runtime_command(build, config, '../escape', [])

    def test_isolated_description(self):
        env = dict(os.environ, PATH='')
        result = subprocess.run([sys.executable, str(CLI), 'describe', '--json'], env=env, capture_output=True, text=True)
        self.assertEqual(result.returncode, 0, result.stderr)
        self.assertEqual(json.loads(result.stdout)['schemaVersion'], 1)


if __name__ == '__main__':
    unittest.main()
