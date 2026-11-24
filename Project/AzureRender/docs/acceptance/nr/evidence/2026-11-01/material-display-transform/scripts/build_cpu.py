import hashlib,json,os,re,subprocess
from pathlib import Path
root=Path.cwd();run=Path(__file__).resolve().parent
files=[p for base in ['src','shaders','tests'] for p in (root/base).rglob('*') if p.is_file() and p.suffix in ['.cpp','.hpp','.inc','.glsl','.frag','.vert','.comp']]
frozen={p.relative_to(root).as_posix():hashlib.sha256(p.read_bytes().replace(b'\r\n',b'\n')).hexdigest() for p in files}
env=os.environ.copy();env.update(VCPKG_ROOT='D:/Tools/vcpkg',VULKAN_SDK='D:/Tools/VulkanSDK/1.4.350.0',DOTNET_ROOT='D:/Tools/dotnet9',DOTNET_CLI_HOME='D:/Tools/dotnet-home',NUGET_PACKAGES='D:/Tools/nuget-packages',DOTNET_PROCESSOR_COUNT='1',PYTHONUTF8='1')
env['PATH']=str(root/'build/reinstall-environment/venv/Scripts')+';D:/Tools/Python311;D:/Tools/dotnet9;D:/Tools/VulkanSDK/1.4.350.0/Bin;'+env['PATH']
cmake=Path('D:/Tools/VS2022BuildTools/Common7/IDE/CommonExtensions/Microsoft/CMake/CMake/bin/cmake.exe');build=root/'build/ninja-msvc-debug'
command=[str(root/'tools/msvc_env.bat'),str(cmake),'--build',str(build),'--target','AzureCharacterRepairTests','AzureMaterialDocumentTests','--parallel','2']
with (run/'cpu-build.log').open('wb') as log:subprocess.run(command,stdout=log,stderr=log,env=env,check=True)
for name,digest in frozen.items():assert hashlib.sha256((root/name).read_bytes().replace(b'\r\n',b'\n')).hexdigest()==digest,name
command=[str(cmake.with_name('ctest.exe')),'--test-dir',str(build),'-R','^(AzureRender.CharacterRepair|AzureAssets.MaterialDocument)$','--no-tests=error','-j','1','--output-on-failure']
with (run/'cpu-green.log').open('wb') as log:subprocess.run(command,stdout=log,stderr=log,env=env,check=True)
assert re.search(r'100% tests passed, 0 tests failed out of 2',(run/'cpu-green.log').read_text())
(run/'cpu-source-freeze.json').write_text(json.dumps({'status':'two-contract-tests-passed','sourceFiles':frozen},indent=2)+'\n');print('PASS two actual contract tests')
