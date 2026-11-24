"""Exercise Windows GUI process waiting and failure propagation."""
import os
from pathlib import Path
import subprocess
import tempfile
import unittest

ROOT = Path(__file__).resolve().parents[1]

@unittest.skipUnless(os.name == 'nt', 'Windows process contract')
class RuntimeCheckTests(unittest.TestCase):
    def test_gui_success_and_nonzero_failures(self):
        with tempfile.TemporaryDirectory(prefix='Azure runtime ') as folder:
            fixture = Path(folder) / '中文 path'
            fixture.mkdir()
            for name, version, resources in [('success', 0, 0), ('version-failure', 7, 0), ('resource-failure', 0, 9)]:
                exe = fixture / (name + '.exe')
                source = ('using System; using System.Threading; public class Entry { '
                          'public static int Main(string[] args) { Thread.Sleep(200); '
                          f'return args[0] == "--version" ? {version} : {resources};' + ' } }')
                compile_script = fixture / 'compile.ps1'
                compile_script.write_text("Add-Type -TypeDefinition '" + source + "' -OutputAssembly '" + str(exe) + "' -OutputType WindowsApplication", encoding='utf-8-sig')
                subprocess.run(['powershell.exe', '-NoProfile', '-File', str(compile_script)], check=True, capture_output=True)
                result = subprocess.run(['powershell.exe', '-NoProfile', '-File', str(ROOT/'tools/verify_windows_runtime.ps1'), '-Executable', str(exe)], capture_output=True, text=True, encoding='utf-8', errors='replace')
                with self.subTest(name=name):
                    if version or resources:
                        self.assertNotEqual(result.returncode, 0)
                        self.assertIn('exit code ' + str(version or resources), result.stderr)
                    else:
                        self.assertEqual(result.returncode, 0, result.stderr)
                        self.assertIn('runtime check passed', result.stdout)

if __name__ == '__main__':
    unittest.main()

