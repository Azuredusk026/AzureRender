from pathlib import Path
import os
import subprocess
import sys

root = Path(__file__).resolve().parent
source = Path('D:/Projects/FYP-worktrees/thesis-paths/Project/AzureRender/experiments/thesis')
env = dict(os.environ, VULKAN_SDK='D:/Tools/VulkanSDK/1.4.350.0', VCPKG_ROOT='D:/Tools/vcpkg', PYTHONUTF8='1')
msvc = 'D:/Projects/FYP/Project/AzureRender/tools/msvc_env.bat'
adb = 'D:/Tools/RenderDoc/1.46/RenderDoc_1.46_64/plugins/android/adb.exe'
def run(name, command, check=True):
    with (root / (name + '.log')).open('w', encoding='utf-8') as log:
        result = subprocess.run(command, env=env, stdout=log, stderr=subprocess.STDOUT, timeout=180)
    print(name, result.returncode, flush=True)
    if check and result.returncode:
        print((root / (name + '.log')).read_text(encoding='utf-8', errors='replace'))
        sys.exit(result.returncode)

run('configure', [msvc, 'cmake', '-S', str(source), '-B', str(root/'build'), '-G', 'Ninja',
                  '-DCMAKE_BUILD_TYPE=Release', '-DPython3_EXECUTABLE='+sys.executable])
run('build', [msvc, 'cmake', '--build', str(root/'build'), '--parallel', '2'])
run('desktop-query', [str(root/'build/AzureThesisQualification.exe'), str(root/'desktop-capabilities.json')])
run('qualification', [sys.executable, '-B', str(source/'tools/qualification.py'), str(root/'desktop-capabilities.json'),
                       '--output', str(root/'desktop-qualification.json')])
run('mobile-properties', [adb, '-s', 'df0a81dc', 'shell', 'getprop'])
run('mobile-vulkan', [adb, '-s', 'df0a81dc', 'shell', 'cmd', 'gpu', 'vkjson'], check=False)
run('ctest', [msvc, 'ctest', '--test-dir', str(root/'build'), '--output-on-failure'])
