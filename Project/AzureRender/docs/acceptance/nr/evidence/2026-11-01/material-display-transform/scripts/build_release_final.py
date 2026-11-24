import hashlib,json,os,re,subprocess,time
from pathlib import Path
root=Path.cwd();run=Path(__file__).resolve().parent
names=['CharacterRepairTests','MaterialDocumentTests','SceneMultisamplingTests','RenderViewTests','RenderViewSubmissionTests','ExtensionRegistryTests','ResourceAccessProfileTests','ViewBatchingTests','TemporalViewStateTests','UnlitTextureMaterialTests','TemporalReconstructionTests','MultisampleViewTests']
cmake_text=(root/'CMakeLists.txt').read_text();mapping={name:re.search(r'add_executable\(\s*(\w+)\s+tests/'+name+r'\.cpp',cmake_text).group(1) for name in names}
targets=['AzurePlayer','AzureRender','AzureMaterialValidator','AzureSceneMultisamplingGpuTests','AzureMaterialDisplayViewGpuTests','AzureTemporalViewGpuTests','AzureMultisampleViewGpuTests',*mapping.values()]
files=[p for base in ['src','shaders','tests'] for p in (root/base).rglob('*') if p.is_file() and p.suffix in ['.cpp','.hpp','.inc','.glsl','.frag','.vert','.comp']]
files+=[root/'CMakeLists.txt',root/'schemas/azure_render_material.schema.json']
frozen={p.relative_to(root).as_posix():hashlib.sha256(p.read_bytes().replace(b'\r\n',b'\n')).hexdigest() for p in files}
env=os.environ.copy();env.update(VCPKG_ROOT='D:/Tools/vcpkg',VULKAN_SDK='D:/Tools/VulkanSDK/1.4.350.0',DOTNET_ROOT='D:/Tools/dotnet9',DOTNET_CLI_HOME='D:/Tools/dotnet-home',NUGET_PACKAGES='D:/Tools/nuget-packages',DOTNET_PROCESSOR_COUNT='1',PYTHONUTF8='1')
env['PATH']=str(root/'build/reinstall-environment/venv/Scripts')+';D:/Tools/Python311;D:/Tools/dotnet9;D:/Tools/VulkanSDK/1.4.350.0/Bin;'+env['PATH']
cmake=Path('D:/Tools/VS2022BuildTools/Common7/IDE/CommonExtensions/Microsoft/CMake/CMake/bin/cmake.exe');build=root/'build/ninja-msvc-release'
report={'status':'building','sourceCommit':subprocess.check_output(['git','rev-parse','HEAD'],text=True).strip(),'sourceFiles':frozen,'targets':targets}
try:
    command=[str(root/'tools/msvc_env.bat'),str(cmake),'--build',str(build),'--target',*targets,'--parallel','2'];report['command']=command
    with (run/'build-release-final.log').open('wb') as log:subprocess.run(command,stdout=log,stderr=log,env=env,check=True)
    for name,digest in frozen.items():assert hashlib.sha256((root/name).read_bytes().replace(b'\r\n',b'\n')).hexdigest()==digest,name
    ctest=str(cmake.with_name('ctest.exe'));listing=json.loads(subprocess.check_output([ctest,'--test-dir',str(build),'--show-only=json-v1'],env=env))
    selected=[row['name'] for row in listing['tests'] if Path(row['command'][0]).stem in mapping.values()];assert len(selected)==len(mapping)
    command=[ctest,'--test-dir',str(build),'-R','^('+'|'.join(re.escape(n) for n in selected)+')$','--no-tests=error','-j','1','--output-on-failure']
    with (run/'cpu-release-final.log').open('wb') as log:subprocess.run(command,stdout=log,stderr=log,env=env,check=True)
    assert re.search(r'100% tests passed, 0 tests failed out of '+str(len(selected)),(run/'cpu-release-final.log').read_text())
    report.update(status='built-contract-tests-passed',tests=selected,programs={n:hashlib.sha256((build/(n+'.exe')).read_bytes()).hexdigest() for n in ['AzurePlayer','AzureRender','AzureMaterialValidator']},
        shaders={p.name:hashlib.sha256(p.read_bytes()).hexdigest() for p in sorted((build/'shaders').glob('*.spv'))})
    print('PASS release final build and',len(selected),'actual contract tests')
except Exception as error:report.update(status='failed',failure=str(error));raise
finally:(run/'release-final-build.json').write_text(json.dumps(report,indent=2)+'\n')
