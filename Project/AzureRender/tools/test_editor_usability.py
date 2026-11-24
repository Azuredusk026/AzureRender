"""Replay editor interactions through the production Dear ImGui host."""
import argparse
import json
import math
import shutil
import time
from pathlib import Path
from create_playable_project import create
from test_editor_workspace import launch as launch_workspace

def launch(*args, **kwargs):
    return launch_workspace(*args, **kwargs, close_policy="discard")


def run(executable, output, workflow):
    output.mkdir(parents=True, exist_ok=True)
    if workflow == 'exploration':
        create(output / 'game')
        other, pick = 'ground', 'hero:body'
    else:
        shutil.copytree(Path(__file__).resolve().parents[1] / 'assets_public/scene_inspector', output / 'game', ignore=shutil.ignore_patterns('.azure'))
        other, pick = 'asset-b', 'asset-a'
    saved = {path: path.read_bytes() for path in (output/'game').rglob('*.azurelevel')}
    baseline = launch(executable, output, 'baseline', (1280, 720), 1, capture=False)
    single = launch(executable, output, 'single', (1280, 720), 1,
                    [dict(frame=25, action='click', target='node.' + other)], capture=False)
    assert single['widgets']['pick.' + pick] == baseline['widgets']['pick.' + pick], 'Single click must preserve camera'
    double = launch(executable, output, 'double', (1280, 720), 1,
                    [dict(frame=25, action='click', target='node.' + other),
                     dict(frame=29, action='click', target='node.' + other)], capture=False)
    assert double['widgets']['pick.' + pick] != baseline['widgets']['pick.' + pick], 'Double click must frame selection'
    events = [dict(frame=25, action='click', target='mode.Scale'),
              dict(frame=29, action='click', target='focus'),
              dict(frame=33, action='key', key='W', down=True),
              dict(frame=34, action='key', key='W', down=False),
              dict(frame=38, action='mouse', target='gizmo.0', down=True),
              dict(frame=41, action='mouse', target='gizmo.0', down=True, offset=[30, 0]),
              dict(frame=45, action='mouse', target='gizmo.0', down=False, offset=[30, 0])]
    moved = launch(executable, output, 'move-key', (1280, 720), 1, events, capture=False)
    assert moved['gizmoTranslation'][0] != 0 and moved['gizmoScale'] == [1, 1, 1], 'W must switch Scale to Move'
    press = launch(executable, output, 'right-press', (1280, 720), 1,
        [dict(frame=25, action='mouse', target='viewport.image', button='right', down=True),
         dict(frame=29, action='mouse', target='viewport.image', button='right', down=False)], capture=False)
    assert press['camera'] == baseline['camera'], 'Navigation capture begins without a view jump'
    nav_events = [dict(frame=21, action='mouse', target='viewport.image', button='right', down=False),
        dict(frame=25, action='mouse', target='viewport.image', button='right', down=True),
        dict(frame=29, action='mouse', target='viewport.image', button='right', down=True, offset=[20,-20]),
        dict(frame=40, action='mouse', target='panelrect.console', button='right', down=False)]
    nav = launch(executable, output, 'right-look-release', (1280,720), 1, nav_events, capture=False)
    before=baseline['camera'];after=nav['camera']
    assert after['position']==before['position'], 'RMB rotation is in place'
    def normalize(v):
        size=math.sqrt(sum(x*x for x in v));return [x/size for x in v]
    forward=normalize([b-a for a,b in zip(before['position'],before['target'])])
    right=normalize([-forward[2],0,forward[0]])
    up=[right[1]*forward[2]-right[2]*forward[1],right[2]*forward[0]-right[0]*forward[2],right[0]*forward[1]-right[1]*forward[0]]
    turned=normalize([b-a for a,b in zip(after['position'],after['target'])])
    assert sum(a*b for a,b in zip(turned,right))>0 and sum(a*b for a,b in zip(turned,up))>0, 'RMB right/up follows local directions'
    assert nav['capture']==dict(navigationButton=-1,gizmo=False)
    fly_events=[dict(frame=18,action='click',target='mode.Scale'),*nav_events[:2],
        dict(frame=28,action='key',key='W',down=True),dict(frame=32,action='key',key='Shift',down=True),
        dict(frame=37,action='key',key='Shift',down=False),dict(frame=43,action='focus',focused=False),
        dict(frame=47,action='key',key='W',down=False),dict(frame=50,action='mouse',target='viewport.image',button='right',down=False)]
    flight=launch(executable,output,'flight-focus',(1280,720),1,fly_events,capture=False)
    assert flight['camera']['position']!=before['position'] and flight['gizmoMode']==2, 'Flight must move without switching tool'
    assert flight['capture']==dict(navigationButton=-1,gizmo=False)
    frames={row['frame']:row for row in flight['history']}
    assert frames[45]['camera']==frames[49]['camera'], 'Focus loss must stop flight'
    closed=launch(executable,output,'close-viewport',(1280,720),1,
        [dict(frame=25,action='mouse',target='viewport.image',button='right',down=True),
         dict(frame=29,action='mouse',target='viewport.image',button='right',down=True,offset=[20,0]),
         dict(frame=33,action='click',target='menu.View'),
         dict(frame=37,action='click',target='panel.viewport'),
         dict(frame=41,action='mouse',target='panelrect.console',button='right',down=False),
         dict(frame=45,action='key',key='W',down=True),
         dict(frame=47,action='key',key='W',down=False)],capture=False)
    assert not closed['panels']['viewport']['open'], 'Viewport close must reach the actual panel'
    assert closed['capture']==dict(navigationButton=-1,gizmo=False), 'Closing viewport releases capture'
    closed_frames={row['frame']:row for row in closed['history']}
    assert closed_frames[41]['camera']==closed_frames[49]['camera'], 'Hidden viewport rejects navigation'
    preview=launch(executable,output,'preview-restore',(1280,720),1,
        [dict(frame=25,action='click',target='play'),dict(frame=40,action='click',target='stop')],capture=False)
    assert preview['camera']==baseline['camera'], 'Play/Stop restores edit camera'
    assert all(path.read_bytes()==content for path,content in saved.items()), 'Navigation and tool selection preserve saved scene bytes'
    summary = dict(passed=True, singlePreservesCamera=True, doubleFramesSelection=True, moveKey=True, navigationDirections=True, focusRelease=True, viewportCloseRelease=True, flight=True, previewCameraRestore=True, sceneBytes=True)
    (output / 'summary.json').write_text(json.dumps(summary, indent=2), encoding='utf-8')
    print(json.dumps(summary))


if __name__ == '__main__':
    parser = argparse.ArgumentParser(__doc__)
    parser.add_argument('--executable', type=Path, required=True)
    parser.add_argument('--output', type=Path, required=True)
    args = parser.parse_args()
    args.output=args.output/ ("run-"+str(time.time_ns()))
    summaries={}
    for workflow in ('exploration','inspector'):
        folder=args.output.resolve()/workflow
        run(args.executable.resolve(), folder, workflow)
        summaries[workflow]=json.loads((folder/'summary.json').read_text(encoding='utf-8'))
    (args.output.resolve()/'summary.json').write_text(json.dumps(summaries,indent=2),encoding='utf-8')
