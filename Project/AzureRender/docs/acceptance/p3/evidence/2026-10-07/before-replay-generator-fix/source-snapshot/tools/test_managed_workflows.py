"""Exercise trusted managed scripts in real Player, preview and movable packages."""
import argparse
import json
import os
from pathlib import Path
import shutil
import subprocess
import sys
import tempfile
import build_game
from run_playable_long_run import isolated_environment

def verify(build,backend,evidence,configuration):
    build=build.resolve();evidence=evidence.resolve();evidence.mkdir(parents=True,exist_ok=True)
    root=Path(tempfile.mkdtemp(prefix='managed-inputs-',dir=evidence))
    products=build/configuration if (build/configuration).is_dir() else build
    player=products/'AzurePlayer.exe';editor=products/'AzureRender.exe'
    results=[]
    def run(name,args,cwd=root,env=None):
        result=subprocess.run(list(map(str,args)),cwd=cwd,env=env,capture_output=True,text=True,encoding='utf-8',errors='replace',timeout=180)
        log=result.stdout+result.stderr;(evidence/(name+'.log')).write_text(log,encoding='utf-8')
        assert result.returncode==0,log[-8000:]
        if name.endswith(('player','preview','moved')):
            assert 'VUID-' not in log and 'Validation Error' not in log and 'Allocator after unload: buffers=0 images=0' in log,log
        return log
    for workflow in ('annotation','controller'):
        project=root/workflow
        run(workflow+'-create',[sys.executable,Path(__file__).with_name('create_managed_project.py'),
            '--player',player,'--artifacts',build/'managed','--backend',backend,'--workflow',workflow,'--output',project])
        run(workflow+'-check',[player,'--project',project/'project.azureproject','--check-project'])
        actions=root/(workflow+'.input.json');actions.write_text(json.dumps({'schemaVersion':1,'actions':[
            {'frame':2,'action':'key','key':87,'down':True},{'frame':10,'action':'key','key':340,'down':True},
            {'frame':60,'action':'key','key':340,'down':False},{'frame':80,'action':'key','key':87,'down':False}]}))
        report=evidence/(workflow+'-player.json')
        run(workflow+'-player',[player,'--project',project/'project.azureproject','--fixed-frame-step','--smoke-frames','100',
            '--game-actions',actions,'--runtime-report',report])
        data=json.loads(report.read_text());hero=next(n for n in data['nodes'] if n['id']=='hero')
        assert data['activeScripts']==1 and not data['scriptErrors'],data
        if workflow=='annotation':assert hero['translation']==[4,5,6],data
        else:assert hero['translation'][2]<-2 and data['animationFrames']>0,data
        if workflow=='annotation':
            task=root/'preview.json';task.write_text(json.dumps([{'frame':2,'command':'play'},{'frame':11,'command':'pause'},{'frame':12,'command':'stop'}]))
            report=evidence/'preview.json'
            run('annotation-preview',[editor,'--editor-project',project/'project.azureproject','--editor-actions',task,
                '--runtime-report',report,'--fixed-frame-step','--smoke-frames','13'])
            preview=json.loads(report.read_text());assert preview['editorPlaySteps']==9 and preview['editStateRestored'] and preview['observedScriptErrors']==0,preview
            assert all(row['passed'] for row in preview['editorActions']),preview
        results.append({'workflow':workflow,'translation':hero['translation'],'activeScripts':data['activeScripts']})
    packageResult=None
    if configuration=='Release':
        engine=root/'engine';run('install',['cmake','--install',build,'--config','Release','--prefix',engine])
        package=evidence/'package';build_game.build_game(root/'annotation/project.azureproject',engine,package,replace=package.exists())
        original=build_game.verify_package(package)
        moved=evidence/'Moved managed game with spaces';assert not moved.exists();package.rename(moved)
        (root/'annotation').rename(root/'Source unavailable');engine.rename(root/'Engine unavailable')
        env=isolated_environment()
        if backend=='nativeaot':env['DOTNET_ROOT']=str(root/'Unavailable dotnet runtime')
        try:
            report=evidence/'moved.json'
            run('annotation-moved',[env['COMSPEC'],'/d','/c','start-game.cmd','--fixed-frame-step','--smoke-frames','20','--runtime-report',report],moved,env)
            data=json.loads(report.read_text());assert not data['scriptErrors'] and data['activeScripts']==1,data
            module=next(p for p in moved.glob('game/assets/managed/*.dll') if p.name.startswith('Azure.Engine'))
            saved=module.read_bytes();module.write_bytes(saved+b' altered')
            try:
                try:build_game.verify_package(moved)
                except ValueError:pass
                else:raise AssertionError('Managed module tamper must be rejected')
            finally:module.write_bytes(saved)
            build_game.verify_package(moved)
            shutil.rmtree(moved/'captures',ignore_errors=True);shutil.rmtree(moved/'game/.azure',ignore_errors=True)
            packageResult={'files':len(original['files']),'moved':True,'isolatedPath':env['PATH'],'sourceUnavailable':True,
                'engineUnavailable':True,'dotnetRuntimeExternal':backend=='coreclr','tamperRejected':True}
        finally:moved.rename(package)
    summary={'status':'passed','backend':backend,'configuration':configuration,'workflows':results,'previewRestored':True,'package':packageResult,'inputs':str(root)}
    (evidence/'summary.json').write_text(json.dumps(summary,indent=2)+'\n');print(json.dumps(summary))

if __name__=='__main__':
    parser=argparse.ArgumentParser(__doc__);parser.add_argument('--build-dir',type=Path,required=True);parser.add_argument('--backend',choices=['coreclr','nativeaot'],required=True)
    parser.add_argument('--evidence',type=Path,required=True);parser.add_argument('--configuration',choices=['Debug','Release'],required=True)
    args=parser.parse_args();verify(args.build_dir,args.backend,args.evidence,args.configuration)
