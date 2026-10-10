"""Exercise empty-scene Add and disabled actions through production ImGui."""
import argparse,json,shutil,uuid
from pathlib import Path
from test_editor_workspace import launch

def run(executable,output):
    root=output/('run-'+uuid.uuid4().hex[:8]);root.mkdir(parents=True)
    source=Path(__file__).resolve().parents[1]/'assets_public'
    results=[]
    for workflow in ('exploration','scene_inspector'):
        for dpi in (1,1.5):
            case=root/(workflow+'-'+str(dpi));case.mkdir()
            shutil.copytree(source/workflow,case/'game',ignore=shutil.ignore_patterns('.azure'))
            config=json.loads((case/'game/project.azureproject').read_text(encoding='utf-8'))
            path=case/'game/assets'/config['startupScene'].split(':/',1)[1]
            level=json.loads(path.read_text(encoding='utf-8'));level['nodes']=[];level['prefabs']=[];level['lights']=[]
            empty=json.dumps(level,indent=2)+'\n';path.write_text(empty,encoding='utf-8')
            disabled=launch(executable,case,'disabled',(1280,720),dpi,[
                dict(frame=25,action='click',target='outliner.actions'),
                dict(frame=30,action='click',target='outliner.delete'),
                dict(frame=35,action='click',target='outliner.duplicate'),
                dict(frame=40,action='key',key='Escape',down=True),
                dict(frame=41,action='key',key='Escape',down=False)],capture=False)
            assert disabled['nodeCount']==0 and disabled['undoCount']==0
            assert disabled['feedback']==[]
            actions=[dict(frame=25,action='click',target='outliner.add'),
                dict(frame=32,action='click',target='outliner.add'),dict(frame=38,action='click',target='save')]
            created=launch(executable,case,'add-root-child',(1280,720),dpi,actions,capture=False)
            assert created['nodeCount']==2 and created['undoCount']==2 and len(created['selection'])==1
            saved=json.loads(path.read_text(encoding='utf-8'))['nodes']
            child=next(n for n in saved if n['id']==created['selection'][0])
            assert child['parentId'] and any(n['id']==child['parentId'] for n in saved),saved
            path.write_text(empty,encoding='utf-8')
            undo=launch(executable,case,'undo-redo',(1280,720),dpi,actions[:-1]+[
                dict(frame=38,action='key',key='Z',ctrl=True,down=True),dict(frame=39,action='key',key='Z',ctrl=True,down=False),
                dict(frame=42,action='key',key='Z',ctrl=True,down=True),dict(frame=43,action='key',key='Z',ctrl=True,down=False),
                dict(frame=46,action='key',key='Y',ctrl=True,down=True),dict(frame=47,action='key',key='Y',ctrl=True,down=False),
                dict(frame=50,action='key',key='Y',ctrl=True,down=True),dict(frame=51,action='key',key='Y',ctrl=True,down=False),
                dict(frame=53,action='key',key='Y',ctrl=False,down=False),dict(frame=56,action='click',target='save')],capture=False)
            assert undo['nodeCount']==2 and undo['undoCount']==2
            history={row['frame']:row for row in undo['history']}
            assert [history[frame]['nodeCount'] for frame in (40,44,48,52)]==[1,0,1,2]
            assert len(json.loads(path.read_text(encoding='utf-8'))['nodes'])==2
            assert undo['feedback']==[]
            results.append(dict(workflow=workflow,dpi=dpi,window=[1280,720],passed=True,
                rootAndChild=True,disabledActions=True,twoUndoRedo=True))
    (root/'summary.json').write_text(json.dumps(dict(passed=True,input='production ImGui replay',
        physicalInput=False,cases=results),indent=2)+'\n',encoding='utf-8')

if __name__=='__main__':
    p=argparse.ArgumentParser(__doc__);p.add_argument('--executable',type=Path,required=True)
    p.add_argument('--output',type=Path,required=True);a=p.parse_args();run(a.executable.resolve(),a.output.resolve())
