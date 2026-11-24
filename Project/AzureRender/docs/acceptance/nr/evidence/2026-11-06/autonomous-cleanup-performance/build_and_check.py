from pathlib import Path
import json,os,subprocess,time
root=Path(__file__).resolve().parents[3]
out=Path(__file__).resolve().parent
selection=json.loads((root/'build/runs/20261105-125529-automatic-acceptance-3d5b54/prepared-resource-paths/selected.json').read_text())
env=os.environ.copy()
env.update(VCPKG_ROOT='D:/Tools/vcpkg',VULKAN_SDK='D:/Tools/VulkanSDK/1.4.350.0',DOTNET_ROOT='D:/Tools/dotnet9',CMAKE_BUILD_PARALLEL_LEVEL='3')
python=str(root/'build/reinstall-environment/venv/Scripts/python.exe')
ctest='D:/Tools/VS2022BuildTools/Common7/IDE/CommonExtensions/Microsoft/CMake/CMake/bin/ctest.exe'
rows=[]
for config in ['Release','Debug']:
    build=root/('build/ninja-msvc-'+config.lower())
    commands=[['cmd','/d','/c',str(root/'tools/msvc_env.bat'),python,'-X','utf8',str(root/'tools/build_provenance.py'),'build','--source',str(root),'--build-dir',str(build),'--config',config,*[arg for target in selection['targets'] for arg in ['--test-target',target]]],
              [ctest,'--test-dir',str(build),'-R','^('+'|'.join(selection['names'])+')$','--output-on-failure','--no-tests=error','--output-junit',str(out/(config.lower()+'-checks.xml'))]]
    for kind,command,deadline in zip(['build','checks'],commands,[900,300]):
        start=time.monotonic()
        with (out/(config.lower()+'-'+kind+'.log')).open('wb') as log:
            result=subprocess.run(command,cwd=root,env=env,stdout=log,stderr=subprocess.STDOUT,timeout=deadline)
        row=dict(configuration=config,kind=kind,exitCode=result.returncode,seconds=time.monotonic()-start,command=command)
        rows.append(row)
        (out/'build-check-results.json').write_text(json.dumps(rows,indent=2))
        assert result.returncode==0,(out/(config.lower()+'-'+kind+'.log')).read_text(errors='replace')[-4000:]
        print(config,kind,'passed',row['seconds'],flush=True)
print('Both configurations built and all selected checks passed',flush=True)
