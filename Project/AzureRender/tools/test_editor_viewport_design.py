"""Check viewport projection controls and image bounds through real UI."""
import argparse
import json
import shutil
import uuid
from pathlib import Path
from test_editor_workspace import launch
from create_playable_project import create


def run(executable, root):
    root = root / ('run-' + uuid.uuid4().hex[:8])
    root.mkdir(parents=True)
    results=[]
    for workflow in ('scene_inspector','exploration'):
        work=root/workflow
        if workflow=='exploration':create(work/'game')
        else:shutil.copytree(Path(__file__).resolve().parents[1]/'assets_public/scene_inspector',work/'game',ignore=shutil.ignore_patterns('.azure'))
        for name,size,dpi in [('normal',(1920,1080),1),('compact',(1120,720),1),('scaled',(1920,1080),1.5)]:
            events=[dict(frame=25,action='click',target='viewport.view-options'),
                    dict(frame=31,action='click',target='viewport.projection.orthographic')]
            data=launch(executable,work,'orthographic-'+name,size,dpi,events,capture=True)
            assert 'viewportView' in data, 'Projection feedback must expose actual camera service state'
            assert data['viewportView']['projection']=='orthographic',data['viewportView']
            image=data['image'];panel=data['panels']['viewport']['rect']
            assert image['width']>0 and image['height']>0
            assert panel[0]<=image['x'] and image['x']+image['width']<=panel[0]+panel[2]+1
            assert panel[1]<=image['y'] and image['y']+image['height']<=panel[1]+panel[3]+1
            for target in ('focus','viewport.view-options','gizmo.options'):
                r=data['widgets'][target]
                assert panel[0]<=r[0] and r[0]+r[2]<=panel[0]+panel[2]+1,(target,panel,r)
            perspective=launch(executable,work,'perspective-'+name,size,dpi,events+[
                dict(frame=39,action='click',target='viewport.view-options'),
                dict(frame=45,action='click',target='viewport.projection.perspective')],capture=False)
            assert perspective['viewportView']['projection']=='perspective' and not perspective['dirty']
            assert perspective['feedback']==[]
            results.append(dict(workflow=workflow,size=size,dpi=dpi,projectionSwitch=True,imageBounds=True,toolbarBounds=True))
    result=dict(passed=True,cases=results,input='production ImGui replay',physicalInput='not performed')
    (root/'summary.json').write_text(json.dumps(result,indent=2),encoding='utf-8')
    print(json.dumps(result))


if __name__=='__main__':
    p=argparse.ArgumentParser(__doc__)
    p.add_argument('--executable',required=True,type=Path)
    p.add_argument('--output',required=True,type=Path)
    args=p.parse_args();run(args.executable.resolve(),args.output.resolve())
