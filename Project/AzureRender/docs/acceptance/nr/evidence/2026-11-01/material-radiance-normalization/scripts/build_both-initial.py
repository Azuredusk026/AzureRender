import hashlib,json,os,re,subprocess,time
from pathlib import Path
root=Path.cwd();run=Path(__file__).resolve().parent
cmake=Path('D:/Tools/VS2022BuildTools/Common7/IDE/CommonExtensions/Microsoft/CMake/CMake/bin/cmake.exe')
ctest=cmake.with_name('ctest.exe')
names=['CharacterRepairTests','MaterialDocumentTests','OutlineProjectionTests','UnlitTextureMaterialTests','TextureMaterialImportTests','MaterialOriginInstanceTests','MaterialGraphTests','MaterialTextureSharingTests','MaterialPipelinePolicyTests','ShadowColorLutTests','SceneInstanceGpuLayoutTests']
text=(root/'CMakeLists.txt').read_text();mapping={name:re.search(r'add_executable\((\w+)\s+tests/'+name+r'\.cpp',text).group(1) for name in names}
targets=['AzurePlayer','AzureRender','AzureMaterialValidator',*mapping.values()]
files=[p for directory in ['src','shaders','tests'] for p in (root/directory).rglob('*') if p.is_file() and p.suffix in ['.cpp','.hpp','.inc','.frag','.vert','.glsl','.comp']]
files+=[root/'CMakeLists.txt',root/'schemas/azure_render_material.schema.json']
fingerprints={p.relative_to(root).as_posix():hashlib.sha256(p.read_bytes().replace(b'\r\n',b'\n')).hexdigest() for p in files}
env=os.environ.copy();env.update(VCPKG_ROOT='D:/Tools/vcpkg',VULKAN_SDK='D:/Tools/VulkanSDK/1.4.350.0',DOTNET_ROOT='D:/Tools/dotnet9',DOTNET_CLI_HOME='D:/Tools/dotnet-home',NUGET_PACKAGES='D:/Tools/nuget-packages',DOTNET_PROCESSOR_COUNT='1',PYTHONUTF8='1')
env['PATH']=str(root/'build/reinstall-environment/venv/Scripts')+';D:/Tools/Python311;D:/Tools/dotnet9;D:/Tools/VulkanSDK/1.4.350.0/Bin;'+env['PATH']
report={'status':'building','sourceCommit':subprocess.check_output(['git','rev-parse','HEAD'],text=True).strip(),'sourceFiles':fingerprints,'targets':targets,'configurations':[]}
startup=subprocess.STARTUPINFO();startup.dwFlags|=subprocess.STARTF_USESHOWWINDOW;startup.wShowWindow=0
try:
    for cfg in ['release','debug']:
        build=root/('build/ninja-msvc-'+cfg);command=[str(root/'tools/msvc_env.bat'),str(cmake),'--build',str(build),'--target',*targets,'--parallel','2']
        print('BUILD',cfg,flush=True);start=time.perf_counter()
        with (run/f'build-final-{cfg}.log').open('wb') as log:subprocess.run(command,stdout=log,stderr=log,env=env,startupinfo=startup,check=True)
        for name,digest in fingerprints.items():assert hashlib.sha256((root/name).read_bytes().replace(b'\r\n',b'\n')).hexdigest()==digest,name
        listing=json.loads(subprocess.check_output([str(ctest),'--test-dir',str(build),'--show-only=json-v1'],env=env))
        selected=[test['name'] for test in listing['tests'] if Path(test['command'][0]).stem in mapping.values()]
        assert len(selected)==len(mapping),(selected,mapping)
        pattern='^(?:'+'|'.join(re.escape(name) for name in selected)+')$'
        with (run/f'cpu-final-{cfg}.log').open('wb') as log:subprocess.run([str(ctest),'--test-dir',str(build),'-R',pattern,'-j','1','--output-on-failure'],stdout=log,stderr=log,env=env,startupinfo=startup,check=True)
        programs={name:hashlib.sha256((build/(name+'.exe')).read_bytes()).hexdigest() for name in ['AzurePlayer','AzureRender','AzureMaterialValidator']}
        row={'configuration':cfg,'status':'built-cpu-passed','seconds':time.perf_counter()-start,'command':command,'cpuTests':selected,'programs':programs}
        report['configurations'].append(row);(run/f'{cfg}-ready.json').write_text(json.dumps(row,indent=2)+'\n')
        print('READY',cfg,len(selected),'CPU tests',flush=True);(run/'frozen-build.json').write_text(json.dumps(report,indent=2)+'\n')
    report['status']='built-cpu-passed'
finally:(run/'frozen-build.json').write_text(json.dumps(report,indent=2)+'\n')
