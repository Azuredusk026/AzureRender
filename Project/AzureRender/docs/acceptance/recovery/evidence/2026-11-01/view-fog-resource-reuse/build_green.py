import hashlib,json,os,re,subprocess
from pathlib import Path
root=Path.cwd();run=Path(__file__).resolve().parent;cmake=Path('D:/Tools/VS2022BuildTools/Common7/IDE/CommonExtensions/Microsoft/CMake/CMake/bin/cmake.exe')
names=['AssetLevelTests','ProjectTests','ProjectStartupTests','ViewFogBindingTests','FogViewProjectionTests','FogVolumeOwnerTests','RenderViewTests','RenderViewSubmissionTests','TemporalViewStateTests','SceneGraphTests'];text=(root/'CMakeLists.txt').read_text();mapping={name:re.search(r'add_executable\(\s*(\w+)\s+tests/'+name+r'\.cpp',text).group(1) for name in names};targets=['AzurePlayer','AzureRender','AzureMaterialValidator',*mapping.values()]
env=os.environ.copy();env.update(VCPKG_ROOT='D:/Tools/vcpkg',VULKAN_SDK='D:/Tools/VulkanSDK/1.4.350.0',DOTNET_ROOT='D:/Tools/dotnet9',DOTNET_CLI_HOME='D:/Tools/dotnet-home',NUGET_PACKAGES='D:/Tools/nuget-packages',DOTNET_PROCESSOR_COUNT='1',PYTHONUTF8='1');env['PATH']=str(root/'build/reinstall-environment/venv/Scripts')+';D:/Tools/Python311;D:/Tools/dotnet9;D:/Tools/VulkanSDK/1.4.350.0/Bin;'+env['PATH']
sha=lambda p:hashlib.sha256(p.read_bytes()).hexdigest();files=[p for base in ['src','shaders','tests'] for p in (root/base).rglob('*') if p.is_file() and p.suffix in ['.cpp','.hpp','.inc','.glsl','.frag','.vert','.comp']]+[root/'CMakeLists.txt',root/'schemas/azure_render_material.schema.json'];frozen={p.relative_to(root).as_posix():hashlib.sha256(p.read_bytes().replace(b'\r\n',b'\n')).hexdigest() for p in files};tool=root/'tools/test_renderer_residency.py';tool_sha=sha(tool)
for cfg in ['release','debug']:
 directory=root/('build/ninja-msvc-'+cfg);report=dict(status='building',sourceCommit=subprocess.check_output(['git','rev-parse','HEAD'],text=True).strip(),sourceFiles=frozen,targets=targets,residencyScriptSha256=tool_sha)
 try:
  command=[str(root/'tools/msvc_env.bat'),str(cmake),'--build',str(directory),'--target',*targets,'--parallel','2'];print('BUILD',cfg,flush=True)
  with (run/('build-'+cfg+'.log')).open('wb') as log:subprocess.run(command,stdout=log,stderr=log,env=env,check=True)
  for name,digest in frozen.items():assert hashlib.sha256((root/name).read_bytes().replace(b'\r\n',b'\n')).hexdigest()==digest,name
  assert sha(tool)==tool_sha
  ctest=str(cmake.with_name('ctest.exe'));listing=json.loads(subprocess.check_output([ctest,'--test-dir',str(directory),'--show-only=json-v1'],env=env));selected=[row['name'] for row in listing['tests'] if Path(row['command'][0]).stem in mapping.values()];assert len(selected)==10
  command=[ctest,'--test-dir',str(directory),'-R','^('+'|'.join(re.escape(name) for name in selected)+')$','--no-tests=error','-j','1','--output-on-failure']
  with (run/('cpu-'+cfg+'.log')).open('wb') as log:subprocess.run(command,stdout=log,stderr=log,env=env,check=True)
  assert '100% tests passed, 0 tests failed out of 10' in (run/('cpu-'+cfg+'.log')).read_text()
  report.update(status='built-contract-tests-passed',tests=selected,programs={name:sha(directory/(name+'.exe')) for name in ['AzurePlayer','AzureRender','AzureMaterialValidator']},shaders={p.name:sha(p) for p in sorted((directory/'shaders').glob('*.spv'))});print('PASS',cfg,'10 checks',flush=True)
 except Exception as error:report.update(status='failed',failure=str(error));raise
 finally:(run/(cfg+'-build.json')).write_bytes((json.dumps(report,indent=2)+'\n').encode())
