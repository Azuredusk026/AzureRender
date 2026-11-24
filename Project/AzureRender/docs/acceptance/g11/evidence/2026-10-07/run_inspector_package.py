import hashlib
import json
from pathlib import Path
import shutil
import subprocess
import sys
import tempfile

root=Path(__file__).resolve().parents[3]
sys.path.insert(0,str(root/'tools'))
import build_game
from run_playable_long_run import isolated_environment
work=root/'build/evolution/g11/inspector-package'
work.mkdir(parents=True,exist_ok=True)
install=root/'build/ninja-msvc-release/release-gate/install-moved'
with tempfile.TemporaryDirectory(prefix='azure_inspection_delivery_') as folder:
    temporary=Path(folder)
    source=temporary/'Project input';engine=temporary/'Engine input'
    shutil.copytree(root/'assets_public/scene_inspector',source,ignore=shutil.ignore_patterns('.azure'))
    shutil.copytree(install,engine)
    package=temporary/'Package output'
    build_game.build_game(source/'project.azureproject',engine,package)
    moved=work/'Moved Scene Inspector'
    if moved.exists():raise FileExistsError(moved)
    shutil.move(str(package),str(moved))
    source.rename(temporary/'Hidden project input');engine.rename(temporary/'Hidden engine input')
    replay=work/'input.json'
    replay.write_text(json.dumps({'schemaVersion':1,'actions':[
        {'frame':3,'action':'key','key':70,'down':True},{'frame':5,'action':'key','key':70,'down':False},
        {'frame':6,'action':'camera','x':30,'y':5,'scroll':1}]}),encoding='utf-8')
    report=work/'runtime.json';timing=work/'timing.json'
    command=[str(moved/'bin/AzurePlayer.exe'),'--project',str(moved/'game/project.azureproject'),
        '--resource-root',str(moved/'share/AzureRender'),'--game-actions',str(replay),'--runtime-report',str(report),
        '--fixed-frame-step','--smoke-frames','30','--gpu-timing','--gpu-timing-output',str(timing),'--width','960','--height','540']
    result=subprocess.run(command,cwd=work,env=isolated_environment(),capture_output=True,text=True,encoding='utf-8',errors='replace',timeout=120)
    (work/'stdout.log').write_text(result.stdout,encoding='utf-8');(work/'stderr.log').write_text(result.stderr,encoding='utf-8')
    assert result.returncode==0,result.stdout+result.stderr
    data=json.loads(report.read_text(encoding='utf-8'))
    assert data['activeScripts']==2 and data['fixedSteps']>0 and not data['scriptErrors'] and not data['presentationErrors']
    assert any(frame['interactionTarget']=='asset-b' for frame in data['routeFrames'])
    assert all(not frame['characters'] for frame in data['routeFrames'])
    memory=json.loads(timing.read_text(encoding='utf-8'))['allocator']
    assert 'Allocator after unload: buffers=0 images=0' in result.stdout,result.stdout
    assert 'Validation Error' not in result.stdout+result.stderr
    build_game.verify_package(moved)
    summary={'status':'passed','path':str(moved),'inputsHidden':True,'isolatedEnvironment':True,
        'manifestSha256':hashlib.sha256((moved/'game_manifest.json').read_bytes()).hexdigest(),'allocatorDuringRun':memory,'released':True}
    (work/'summary.json').write_text(json.dumps(summary,indent=2)+'\n',encoding='utf-8')
    print(json.dumps(summary))
