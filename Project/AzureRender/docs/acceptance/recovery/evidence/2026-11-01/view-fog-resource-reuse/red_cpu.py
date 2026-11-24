import hashlib,json,os,re,subprocess
from pathlib import Path
root=Path.cwd();run=Path(__file__).resolve().parent;cmake=Path('D:/Tools/VS2022BuildTools/Common7/IDE/CommonExtensions/Microsoft/CMake/CMake/bin/cmake.exe');directory=root/'build/ninja-msvc-release'
env=os.environ.copy();env.update(VCPKG_ROOT='D:/Tools/vcpkg',VULKAN_SDK='D:/Tools/VulkanSDK/1.4.350.0',DOTNET_ROOT='D:/Tools/dotnet9',DOTNET_CLI_HOME='D:/Tools/dotnet-home',NUGET_PACKAGES='D:/Tools/nuget-packages',DOTNET_PROCESSOR_COUNT='1',PYTHONUTF8='1');env['PATH']=str(root/'build/reinstall-environment/venv/Scripts')+';D:/Tools/Python311;D:/Tools/dotnet9;D:/Tools/VulkanSDK/1.4.350.0/Bin;'+env['PATH']
files=[p for base in ['src','shaders','tests'] for p in (root/base).rglob('*') if p.is_file() and p.suffix in ['.cpp','.hpp','.inc','.glsl','.frag','.vert','.comp']]+[root/'CMakeLists.txt',root/'schemas/azure_render_material.schema.json'];frozen={p.relative_to(root).as_posix():hashlib.sha256(p.read_bytes().replace(b'\r\n',b'\n')).hexdigest() for p in files}
command=[str(root/'tools/msvc_env.bat'),str(cmake),'--build',str(directory),'--target','AzureAssetLevelTests','--parallel','2']
with (run/'red-cpu-build.log').open('wb') as log:subprocess.run(command,stdout=log,stderr=log,env=env,check=True)
for name,digest in frozen.items():assert hashlib.sha256((root/name).read_bytes().replace(b'\r\n',b'\n')).hexdigest()==digest,name
command=[str(cmake.with_name('ctest.exe')),'--test-dir',str(directory),'-R','^AzureEngine[.]AssetLevel$','--no-tests=error','--output-on-failure']
with (run/'red-cpu-test.log').open('wb') as log:result=subprocess.run(command,stdout=log,stderr=log,env=env)
text=(run/'red-cpu-test.log').read_text();assert result.returncode!=0 and 'Asset/level contract failed at line' in text and 'out of 1' in text,text[-1500:]
report=dict(status='red-confirmed',sourceCommit=subprocess.check_output(['git','rev-parse','HEAD'],text=True).strip(),sourceFiles=frozen,exitCode=result.returncode,test='AzureEngine.AssetLevel',executableSha256=hashlib.sha256((directory/'AzureAssetLevelTests.exe').read_bytes()).hexdigest())
(run/'red-cpu.json').write_bytes((json.dumps(report,indent=2)+'\n').encode());(run/'.keep').write_text('View fog resource identity and lifecycle evidence.\n');print('RED confirmed: captured fog camera changes retain the old resource key')
