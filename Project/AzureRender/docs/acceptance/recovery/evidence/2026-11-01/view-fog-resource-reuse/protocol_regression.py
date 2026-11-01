import hashlib,json,os,subprocess
from pathlib import Path
root=Path.cwd();run=Path(__file__).resolve().parent;cmake=Path('D:/Tools/VS2022BuildTools/Common7/IDE/CommonExtensions/Microsoft/CMake/CMake/bin/cmake.exe');env=os.environ.copy();env.update(VCPKG_ROOT='D:/Tools/vcpkg',VULKAN_SDK='D:/Tools/VulkanSDK/1.4.350.0',DOTNET_ROOT='D:/Tools/dotnet9',DOTNET_CLI_HOME='D:/Tools/dotnet-home',NUGET_PACKAGES='D:/Tools/nuget-packages',DOTNET_PROCESSOR_COUNT='1',PYTHONUTF8='1');env['PATH']='D:/Tools/Python311;D:/Tools/dotnet9;D:/Tools/VulkanSDK/1.4.350.0/Bin;'+env['PATH'];rows=[]
for cfg in ['release','debug']:
 directory=root/('build/ninja-msvc-'+cfg)
 with (run/('protocol-build-'+cfg+'.log')).open('wb') as log:subprocess.run([str(root/'tools/msvc_env.bat'),str(cmake),'--build',str(directory),'--target','AzureValidationProbe','--parallel','2'],env=env,stdout=log,stderr=log,check=True)
 with (run/('protocol-test-'+cfg+'.log')).open('wb') as log:subprocess.run([str(cmake.with_name('ctest.exe')),'--test-dir',str(directory),'-R','^AzureTools[.]ValidationProtocol$','--no-tests=error','--output-on-failure'],env=env,stdout=log,stderr=log,check=True)
 assert '100% tests passed, 0 tests failed out of 1' in (run/('protocol-test-'+cfg+'.log')).read_text()
 rows.append(dict(configuration=cfg,status='passed',probeSha256=hashlib.sha256((directory/'AzureValidationProbe.exe').read_bytes()).hexdigest()))
(run/'protocol-summary.json').write_bytes((json.dumps(dict(status='passed',checks=rows),indent=2)+'\n').encode());print('Protocol compatibility passed in both configurations')
