"""Exercise the scene inspection workflow through production editor and Player hosts."""
import argparse
import json
from pathlib import Path
import shutil
import subprocess
import tempfile

def execute(command, cwd):
    result=subprocess.run(list(map(str,command)),cwd=cwd,capture_output=True,text=True,encoding='utf-8',errors='replace',timeout=120)
    assert result.returncode==0,f'Exit {result.returncode}\n'+result.stdout+result.stderr
    assert 'Validation Error' not in result.stdout+result.stderr,result.stdout+result.stderr
    return result.stdout+result.stderr

def main():
    parser=argparse.ArgumentParser(__doc__)
    parser.add_argument('--editor',type=Path,required=True)
    parser.add_argument('--player',type=Path,required=True)
    parser.add_argument('--project',type=Path,required=True)
    parser.add_argument('--output',type=Path)
    args=parser.parse_args()
    with tempfile.TemporaryDirectory(prefix='azure_inspector_') as temporary:
        root=Path(temporary);project=root/'Scene inspector project'
        shutil.copytree(args.project.resolve().parent,project,ignore=shutil.ignore_patterns('.azure'))
        config=json.loads((project/'project.azureproject').read_text(encoding='utf-8'))
        assert [s['id'] for s in config['runtime']['systems']]==['camera','interaction']
        inputs=root/'input.json'
        inputs.write_text(json.dumps({'schemaVersion':1,'actions':[
            {'frame':5,'action':'key','key':70,'down':True},
            {'frame':7,'action':'key','key':70,'down':False},
            {'frame':8,'action':'camera','x':30,'y':5,'scroll':1}]}),encoding='utf-8')
        reports={}
        for mode,executable in [('editor',args.editor),('player',args.player)]:
            report=root/(mode+'.json')
            command=[executable.resolve(),'--editor-project' if mode=='editor' else '--project',project/'project.azureproject',
                '--game-actions',inputs,'--runtime-report',report,'--fixed-frame-step','--smoke-frames','24','--width','960','--height','540']
            if mode=='editor':
                actions=root/'editor-actions.json'
                actions.write_text(json.dumps([{'frame':1,'command':'select','id':'asset-a'},
                    {'frame':2,'command':'save'},{'frame':3,'command':'reload'},
                    {'frame':4,'command':'play'},{'frame':18,'command':'stop'}]),encoding='utf-8')
                command+=['--editor-actions',actions]
            log=execute(command,root);data=json.loads(report.read_text(encoding='utf-8'))
            assert not data['scriptErrors'] and not data['presentationErrors'],data
            routes=data['routeFrames'];assert routes and all(not frame['characters'] for frame in routes),data
            assert any(frame['interactionTarget']=='asset-a' for frame in routes),data
            assert any(frame['interactionTarget']=='asset-b' for frame in routes),data
            assert routes[0]['camera']!=routes[-1]['camera'],data
            if mode=='editor':
                assert data['editorPlaySteps']>0 and data['editStateRestored'],data
                assert data['editorWorkspace']['panels']['viewport']['open'],data
                assert 'pick.asset-a' in data['editorWorkspace']['widgets'],data
                assert all(item['passed'] for item in data['editorActions']),data
            else:
                assert data['fixedSteps']>0 and data['activeScripts']==2 and data['replayedActions']==3,data
            reports[mode]=data
            if args.output:
                args.output.mkdir(parents=True,exist_ok=True)
                (args.output/(mode+'.json')).write_text(json.dumps(data,indent=2)+'\n',encoding='utf-8')
                (args.output/(mode+'.log')).write_text(log,encoding='utf-8')
        print(json.dumps({'status':'passed','workflows':list(reports),'keyboardMouse':'production replay; physical devices not exercised'}))

if __name__=='__main__':
    main()
