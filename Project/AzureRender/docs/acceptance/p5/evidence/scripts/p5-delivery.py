"""Publish, relocate and verify current games on NVIDIA and Intel."""
import gzip
import hashlib
import json
import os
from pathlib import Path
import shutil
import subprocess
import sys
import zipfile

ROOT = Path.cwd().resolve()
sys.path.insert(0, str(ROOT/'tools'))
from build_game import build_game, verify_package
from build_provenance import verify
from run_playable_long_run import isolated_environment

OUT = ROOT/'build/implementation-2026-10-10/p5-delivery'
OUT.mkdir(parents=True, exist_ok=False)
RELEASE = ROOT/'build/ninja-msvc-release'
proof = json.loads((RELEASE/'build-provenance.json').read_text(encoding='utf-8'))
verify(ROOT, proof)
def digest(path): return hashlib.sha256(path.read_bytes()).hexdigest()
def command(name, args, cwd=OUT, env=None, timeout=900):
    startup=subprocess.STARTUPINFO();startup.dwFlags|=subprocess.STARTF_USESHOWWINDOW;startup.wShowWindow=0
    result=subprocess.run(list(map(str,args)),cwd=cwd,env=env,capture_output=True,text=True,
                          encoding='utf-8',errors='replace',timeout=timeout,startupinfo=startup)
    (OUT/(name+'.stdout.log')).write_text(result.stdout,encoding='utf-8')
    (OUT/(name+'.stderr.log')).write_text(result.stderr,encoding='utf-8')
    assert result.returncode==0,(name,result.stdout[-3000:]+result.stderr[-3000:])
    return result

engine=OUT/'Engine input'
command('install',['cmake','--install',RELEASE,'--config','Release','--prefix',engine])
final_engine=OUT/'Moved Engine'
engine.rename(final_engine)
for name in ('AzureRender','AzurePlayer'):
    command('isolated-'+name,['powershell.exe','-NoProfile','-ExecutionPolicy','Bypass','-File',
        ROOT/'tools/verify_windows_runtime.ps1','-Executable',final_engine/'bin'/(name+'.exe')])
for relative in ('contracts/manifest.json','licenses/ImViewGuizmo-LICENSE.txt',
                 'docs/runtime/viewport-navigation.md','docs/runtime/platformer-gameplay.md'):
    assert (final_engine/'share/AzureRender'/relative).is_file(),relative

sources={name:ROOT/'assets_public'/name for name in ('exploration','platformer','scene_inspector')}
sources.update({ 'authored-'+name:ROOT/'build/implementation-2026-10-10/p5-workflows-retry2'/name
                for name in ('exploration','inspection') })
for name, original in sources.items():
    source=OUT/('Project input '+name)
    shutil.copytree(original,source,ignore=shutil.ignore_patterns('.azure'))
    build_game(source/'project.azureproject',final_engine,OUT/('Output '+name))
    (OUT/('Output '+name)).rename(OUT/('Moved '+name))
    source.rename(OUT/('Unavailable project '+name))
final_engine.rename(OUT/'Unavailable Engine')
packages={name:OUT/('Moved '+name) for name in sources}
sequence=max((RELEASE/'game-sequence').glob('sequence-*/sequence-summary.json'),key=lambda p:p.stat().st_mtime).parent
platform=max((RELEASE/'platformer-completion').glob('run-*/gpu-summary.json'),key=lambda p:p.stat().st_mtime).parent
for name, source in [('sequence',sequence),('platformer',platform)]:
    shutil.copy2(source/'input.json',OUT/(name+'.input.json'))
frames={name:json.loads((source/summary).read_text(encoding='utf-8'))['frames']
        for name,source,summary in [('sequence',sequence,'sequence-summary.json'),('platformer',platform,'gpu-summary.json')]}
plans={name:json.loads((source/'summary.json').read_text(encoding='utf-8'))
       for name,source in [('sequence',sequence),('platformer',platform)]}
drivers=json.loads((ROOT/'docs/acceptance/p1/evidence/2026-10-05/driver-inventory.json').read_text(encoding='utf-8'))
intel=next(row for row in drivers if row['DriverDesc']=='Intel(R) UHD Graphics')
assert digest(Path(intel['VulkanDriverName']))==intel['manifestSha256']
rows=[]
for device in ('nvidia','intel'):
    env=isolated_environment()
    env.update(VK_INSTANCE_LAYERS='VK_LAYER_KHRONOS_validation',VK_LAYER_VALIDATE_SYNC='1',VK_LOADER_DEBUG='error,layer')
    if device=='intel': env.update(VK_DRIVER_FILES=intel['VulkanDriverName'],VK_ICD_FILENAMES=intel['VulkanDriverName'])
    for name,package in packages.items():
        case=OUT/(device+'-'+name);case.mkdir()
        actions=OUT/('sequence.input.json' if name=='exploration' else 'platformer.input.json')
        count=frames['sequence' if name=='exploration' else 'platformer']
        fps=plans['sequence' if name=='exploration' else 'platformer']['fps']
        if name=='scene_inspector' or name.startswith('authored-'):
            actions=case/'input.json'
            actions.write_text(json.dumps(dict(schemaVersion=1,actions=[
                dict(frame=3,action='key',key=70,down=True),dict(frame=5,action='key',key=70,down=False),
                dict(frame=6,action='camera',x=30,y=5,scroll=1)])),encoding='utf-8')
            count=30
            fps=60
        report=case/'runtime.json';timing=case/'timing.json'
        result=command(device+'-'+name,[package/'bin/AzurePlayer.exe','--project',package/'game/project.azureproject',
            '--resource-root',package/'share/AzureRender','--game-actions',actions,'--runtime-report',report,
            '--fixed-frame-step','--capture-fps',str(fps),'--smoke-frames',str(count),
            '--width','960','--height','540','--gpu-timing','--gpu-timing-output',timing],cwd=case,env=env)
        logs=result.stdout+result.stderr
        assert 'Allocator after unload: buffers=0 images=0' in result.stdout
        assert 'VK_LAYER_KHRONOS_validation' in logs
        assert not any(s in logs.lower() for s in ('vuid-','validation error','sync-hazard','hazard detected','failed to load layer'))
        data=json.loads(report.read_text(encoding='utf-8'))
        assert not data['scriptErrors'] and not data['presentationErrors']
        cap=json.loads((case/'captures/gpu_capabilities.json').read_text(encoding='utf-8'))
        assert ('Intel' if device=='intel' else 'NVIDIA') in cap['device_name'],cap
        complete=[f for f in data['routeFrames'] if any(t['completed'] for t in f['tasks'])]
        if name=='exploration':
            assert any(f['tasks'][0]['node']=='hero:body' and f['tasks'][0]['collected']==3 for f in complete)
            assert any(f['tasks'][0]['node']=='hero' and f['tasks'][0]['collected']==8 for f in complete)
        elif name=='platformer':
            assert any(f['tasks'][0]['collected']==8 for f in complete)
            assert data['routeFrames'][-1]['collectibles']==10
            assert data['audioStarts']>=20
            route=data['routeFrames']
            hero=lambda f:next(c for c in f['characters'] if c['node']=='hero')
            assert any(hero(f)['position'][1]<-4 for f in route)
            assert any(f['checkpointActivated'] for f in route)
            assert route[-1]['levelRevision']==route[0]['levelRevision']+1
            for landing in plans['platformer']['landings']:
                near=[f for f in route if abs(f['frame']-landing['frame'])<=3]
                assert any(hero(f)['grounded'] and abs(hero(f)['position'][2]-landing['position'][2])<1 for f in near)
            for node,axis in [('platform-3',0),('platform-5',1)]:
                positions=[next(b for b in f['rigidBodies'] if b['node']==node)['position'][axis] for f in route]
                assert max(positions)-min(positions)>.25
        elif name=='scene_inspector':
            assert data['activeScripts']==2 and data['fixedSteps']>0
            assert any(f['interactionTarget']=='asset-b' for f in data['routeFrames'])
            assert all(not f['characters'] for f in data['routeFrames'])
        else:
            assert data['fixedSteps']>0 and data['routeFrames']
            config=json.loads((package/'game/project.azureproject').read_text(encoding='utf-8'))
            level=package/'game/assets'/config['startupScene'].split(':/',1)[1]
            assert digest(level)==digest(sources[name]/'assets'/level.name)
        verify_package(package)
        (case/'runtime.json.gz').write_bytes(gzip.compress(report.read_bytes(),mtime=0))
        rows.append(dict(device=device,workflow=name,passed=True,released=True,validationLayer=True,
            actualDevice=cap['device_name'],frames=count,isolatedPath=env['PATH'],inputsUnavailable=True,
            runtimeSha256=digest(report),capabilities=cap))
        (OUT/'partial.json').write_text(json.dumps(rows,indent=2)+'\n',encoding='utf-8')
        print(device,name,'complete and released',flush=True)
    if device=='nvidia':
        for name,package in packages.items():
            entry=command('entry-'+name,[env['COMSPEC'],'/d','/c','start-game.cmd','--fixed-frame-step','--smoke-frames','4'],cwd=package,env=env)
            assert 'Allocator after unload: buffers=0 images=0' in entry.stdout
            verify_package(package)

hidden_engine=OUT/'Unavailable Engine'
hidden_engine.rename(final_engine)
manifest=final_engine/'install_manifest.json'
command('manifest-write',['cmake','-DINSTALL_DIR='+str(final_engine),'-DOUTPUT_FILE='+str(manifest),'-P',ROOT/'tools/write_install_manifest.cmake'])
command('manifest-verify',['cmake','-DINSTALL_DIR='+str(final_engine),'-DMANIFEST_FILE='+str(manifest),'-P',ROOT/'tools/verify_install_manifest.cmake'])
archives=OUT/'packages';archives.mkdir()
inventory=[]
for name, directory in [('engine',final_engine),*packages.items()]:
    if name!='engine':verify_package(directory)
    archive=archives/('Azure-P5-candidate-'+name+'.zip')
    files={p.relative_to(directory).as_posix():digest(p) for p in directory.rglob('*') if p.is_file() and 'captures' not in p.relative_to(directory).parts and '.azure' not in p.relative_to(directory).parts}
    with zipfile.ZipFile(archive,'w',compression=zipfile.ZIP_DEFLATED,compresslevel=6) as stream:
        for relative in sorted(files):stream.write(directory/relative,directory.name+'/'+relative)
    with zipfile.ZipFile(archive) as stream:
        assert len(stream.namelist())==len(files)
        for relative,expected in files.items():assert hashlib.sha256(stream.read(directory.name+'/'+relative)).hexdigest()==expected
    inventory.append(dict(kind=name,path=str(archive),sha256=digest(archive),bytes=archive.stat().st_size,files=len(files)))
portable=dict(schemaVersion=1,status='MachineVerifiedPhysicalInputPending',configuration='Release',baseCommit=proof['commit'],
    source=proof['source'],products={Path(n).relative_to(RELEASE).as_posix():h for n,h in proof['products'].items()},packages=inventory)
(archives/'provenance.json').write_text(json.dumps(portable,indent=2)+'\n',encoding='utf-8')
summary=dict(passed=True,engine=str(final_engine),packages=inventory,checks=rows,intelDriver=intel,
             manualInput='pending',subjectiveAudio='pending')
(OUT/'summary.json').write_text(json.dumps(summary,indent=2)+'\n',encoding='utf-8')
print(json.dumps(dict(passed=True,packages=inventory)),flush=True)
