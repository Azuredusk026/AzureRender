"""Exercise the actual CMake registration and MSVC environment contract."""
import json
import os
from pathlib import Path
import subprocess
import tempfile
import unittest

ROOT = Path(__file__).resolve().parents[1]


class BuildConfigurationTests(unittest.TestCase):
    @unittest.skipUnless(os.name == "nt", "Windows environment contract")
    def test_msvc_environment_preserves_requested_vcpkg(self):
        with tempfile.TemporaryDirectory() as temp:
            probe = Path(temp) / "probe.py"
            probe.write_text('import os; print(os.environ["VCPKG_ROOT"])')
            result = subprocess.run([str(ROOT / "tools/msvc_env.bat"), "python", str(probe)],
                cwd=ROOT, capture_output=True, text=True)
        self.assertEqual(result.returncode, 0, result.stderr)
        self.assertEqual(Path(result.stdout.strip()).resolve(), Path(os.environ["VCPKG_ROOT"]).resolve())

    def test_release_package_is_discovered_for_multiconfig(self):
        with tempfile.TemporaryDirectory() as temp:
            source = Path(temp)
            (source / "CMakeLists.txt").write_text('''cmake_minimum_required(VERSION 3.20)
project(Registration NONE)
enable_testing()
set(WIN32 TRUE)
add_executable(AzureRender IMPORTED)
set_target_properties(AzureRender PROPERTIES IMPORTED_LOCATION "editor")
set(Python3_EXECUTABLE "python")
include("''' + (ROOT / "tools/register_game_package.cmake").as_posix() + '''")
azure_register_game_package()
''')
            for generator in ["Ninja", "Ninja Multi-Config"]:
                build = source / generator.replace(" ", "-")
                subprocess.run(["cmake", "-S", str(source), "-B", str(build), "-G", generator,
                    "-DCMAKE_BUILD_TYPE=Release"], check=True, capture_output=True)
                report = json.loads(subprocess.check_output(["ctest", "--test-dir", str(build),
                    "-C", "Release", "--show-only=json-v1"], text=True))
                self.assertIn("AzureEngine.GamePackage", [t["name"] for t in report["tests"]])
                if generator == "Ninja Multi-Config":
                    debug = json.loads(subprocess.check_output(["ctest", "--test-dir", str(build),
                        "-C", "Debug", "--show-only=json-v1"], text=True))
                    self.assertNotIn("AzureEngine.GamePackage", [t["name"] for t in debug["tests"]])


if __name__ == "__main__":
    unittest.main()
