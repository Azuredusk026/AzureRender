"""Verify moved engine and two independently packaged projects."""
import hashlib
import json
import os
from pathlib import Path
import shutil
import subprocess
import sys
import tempfile
import time

ROOT=Path.cwd().resolve()
sys.path.insert(0,str(ROOT/'tools'))
from build_game import build_game,verify_package
from create_playable_project import create
from run_playable_long_run import isolated_environment
from test_playable_player import run as quest

OUT=ROOT/'build/p4/delivery'/('run-'+str(time.time_ns()))
OUT.mkdir(parents=True)
def command(name,args,**kwargs):
    result=subprocess.run([str(a) for a in args],capture_output=True,text=True,encoding='utf-8',errors='replace',timeout=600,**kwargs)
    (OUT/(name+'.stdout.log')).write_text(result.stdout,encoding='utf-8')
    (OUT/(name+'.stderr.log')).write_text(result.stderr,encoding='utf-8')
    assert result.returncode==0,result.stdout+result.stderr
    return result

def inspector(package,output,env):
    output.mkdir(parents=True)
    replay=output/'input.json'
    replay.write_text(json.dumps({'schemaVersion':1,'actions':[
        {'frame':3,'action':'key','key':70,'down':True},{'frame':5,'action':'key','key':70,'down':False},
        {'frame':6,'action':'camera','x':30,'y':5,'scroll':1}]}),encoding='utf-8')
    report=output/'runtime.json';timing=output/'timing.json'
    result=command('inspector-'+output.name,[package/'bin/AzurePlayer.exe','--project',package/'game/project.azureproject',
        '--resource-root',package/'share/AzureRender','--game-actions',replay,'--runtime-report',report,
        '--fixed-frame-step','--smoke-frames','30','--gpu-timing','--gpu-timing-output',timing,'--width','960','--height','540'],cwd=output,env=env)
    data=json.loads(report.read_text(encoding='utf-8'))
    assert data['activeScripts']==2 and data['fixedSteps']>0 and not data['scriptErrors'] and not data['presentationErrors']
    assert any(f['interactionTarget']=='asset-b' for f in data['routeFrames'])
    assert all(not f['characters'] for f in data['routeFrames'])
    assert 'Allocator after unload: buffers=0 images=0' in result.stdout
    assert not any(s in result.stdout+result.stderr for s in ('VUID-','Validation Error','SYNC-HAZARD'))
    capabilities=json.loads((output/'captures/gpu_capabilities.json').read_text(encoding='utf-8'))
    return dict(passed=True,released=True,device=capabilities['device_name'])

def main():
    install=OUT/'Engine installation'
    command('install',['cmake','--install',ROOT/'build/ninja-msvc-release','--config','Release','--prefix',install])
    moved=OUT/'Moved Engine';install.rename(moved)
    manifest=moved/'install_manifest.json'
    command('manifest-write',['cmake','-DINSTALL_DIR='+str(moved),'-DOUTPUT_FILE='+str(manifest),'-P',ROOT/'tools/write_install_manifest.cmake'])
    command('manifest-verify',['cmake','-DINSTALL_DIR='+str(moved),'-DMANIFEST_FILE='+str(manifest),'-P',ROOT/'tools/verify_install_manifest.cmake'])
    for target in ('AzureRender','AzurePlayer'):
        command('isolated-'+target,['powershell.exe','-NoProfile','-ExecutionPolicy','Bypass','-File',ROOT/'tools/verify_windows_runtime.ps1','-Executable',moved/'bin'/(target+'.exe')])
    resource=moved/'share/AzureRender'
    for relative in ('docs/tutorials/editor-interaction.md','docs/runtime/content-workflows.md',
        'docs/plans/2026-10-08-editor-acceptance.md','docs/acceptance/p4/2026-10-08.md',
        'licenses/ImGuizmo-LICENSE.txt','licenses/imgui-LICENSE.txt'):
        assert (resource/relative).is_file(),relative
    rows=[]
    with tempfile.TemporaryDirectory(prefix='azure_p4_delivery_') as temporary:
        temp=Path(temporary);engine=temp/'Engine input';shutil.copytree(moved,engine)
        for workflow in ('exploration','inspector'):
            source=temp/('Project '+workflow)
            if workflow=='exploration':create(source)
            else:shutil.copytree(ROOT/'assets_public/scene_inspector',source,ignore=shutil.ignore_patterns('.azure'))
            package=temp/('Output '+workflow);build_game(source/'project.azureproject',engine,package)
            final=OUT/('Moved '+workflow);shutil.move(str(package),str(final))
            source.rename(temp/('Hidden '+workflow))
            engine.rename(temp/'Hidden engine')
            env=isolated_environment()
            if workflow=='exploration':
                quest(final/'bin/AzurePlayer.exe',OUT/'exploration',project=final/'game/project.azureproject',env=env,cwd=OUT)
                result=json.loads((OUT/'exploration/summary.json').read_text(encoding='utf-8'))
            else:result=inspector(final,OUT/'inspector/nvidia',env)
            verify_package(final)
            rows.append(dict(workflow=workflow,path=str(final),inputsHidden=True,isolatedEnvironment=True,result=result,
                manifestSha256=hashlib.sha256((final/'game_manifest.json').read_bytes()).hexdigest()))
            (temp/'Hidden engine').rename(engine)
    summary=dict(passed=True,engine=str(moved),installManifestSha256=hashlib.sha256(manifest.read_bytes()).hexdigest(),packages=rows)
    (OUT/'summary.json').write_text(json.dumps(summary,indent=2),encoding='utf-8')
    print(json.dumps(summary),flush=True)
    return summary

if __name__=='__main__':main()
