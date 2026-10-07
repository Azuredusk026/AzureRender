"""Exercise production gizmo capture, cancellation, snapping and undo through UI input."""
import argparse
import json
import math
import shutil
import time
from pathlib import Path
from create_playable_project import create
from test_editor_workspace import launch


def run(executable, root, workflow):
    root.mkdir(parents=True)
    if workflow == 'exploration':
        create(root / 'game')
        identity = 'hero:body'
    else:
        shutil.copytree(Path(__file__).resolve().parents[1] / 'assets_public/scene_inspector', root / 'game',
                        ignore=shutil.ignore_patterns('.azure'))
        descriptor = json.loads((root / 'game/project.azureproject').read_text(encoding='utf-8'))
        document = json.loads((root / 'game/assets' / descriptor['startupScene'].removeprefix('assets:/')).read_text(encoding='utf-8'))
        identity = next(node['id'] for node in document['nodes'] if node.get('resourceId'))
    start = [dict(frame=25, action='click', target='node.' + identity),
             dict(frame=28, action='click', target='focus'),
             dict(frame=30, action='click', target='mode.Move')]
    drag = [dict(frame=35, action='mouse', target='gizmo.0', down=True),
            dict(frame=38, action='mouse', target='gizmo.0', down=True, offset=[120, 0])]
    baseline = launch(executable, root, 'baseline', (1920, 1080), 1, start, capture=False, close_policy='discard')
    edited = launch(executable, root, 'commit', (1920, 1080), 1, start + drag + [
        dict(frame=41, action='mouse', target='gizmo.0', down=False, offset=[120, 0])],
        capture=False, close_policy='discard')
    assert edited['gizmoTranslation'] != baseline['gizmoTranslation'], 'Actual handle drag must transform the object'
    assert edited['undoCount'] == baseline['undoCount'] + 1, 'Drag creates exactly one undo entry'
    assert not edited['capture']['gizmo']
    for name, ending in [
        ('escape', [dict(frame=40, action='key', key='Escape'), dict(frame=41, action='key', key='Escape', down=False)]),
        ('blur', [dict(frame=40, action='focus', focused=False)]),
    ]:
        data = launch(executable, root, name, (1920, 1080), 1, start + drag + ending,
                      capture=False, close_policy='discard')
        assert data['gizmoTranslation'] == baseline['gizmoTranslation'], 'Cancellation restores authored transform'
        assert data['undoCount'] == baseline['undoCount'] and data['dirty'] == baseline['dirty']
        assert not data['capture']['gizmo'], 'Cancellation releases capture'
    undo = launch(executable, root, 'undo', (1920, 1080), 1, start + drag + [
        dict(frame=41, action='mouse', target='gizmo.0', down=False, offset=[120, 0]),
        dict(frame=44, action='key', key='Z', ctrl=True), dict(frame=45, action='key', key='Z', down=False, ctrl=True)],
        capture=False, close_policy='discard')
    assert undo['gizmoTranslation'] == baseline['gizmoTranslation'] and undo['dirty'] == baseline['dirty']
    snapped = launch(executable, root, 'snap', (1920, 1080), 1, start + [
        dict(frame=35, action='mouse', target='gizmo.0', down=True, ctrl=True),
        dict(frame=38, action='mouse', target='gizmo.0', down=True, offset=[120, 0], ctrl=True),
        dict(frame=41, action='mouse', target='gizmo.0', down=False, offset=[120, 0], ctrl=True)],
        capture=False, close_policy='discard')
    delta = snapped['gizmoTranslation'][0] - baseline['gizmoTranslation'][0]
    assert abs(delta) > .001 and abs(delta / .5 - round(delta / .5)) < .001, 'Ctrl obeys the registered translation snap step'
    # Independent projection places the pointer inside the visible YZ plane handle.
    def normalized(vector):
        length = math.sqrt(sum(v*v for v in vector))
        return [v/length for v in vector]
    camera = baseline['camera']; image = baseline['image']; center = baseline['gizmoTranslation']
    forward = normalized([b-a for a,b in zip(camera['position'],camera['target'])])
    right = normalized([-forward[2],0,forward[0]])
    up = [right[1]*forward[2]-right[2]*forward[1],right[2]*forward[0]-right[0]*forward[2],right[0]*forward[1]-right[1]*forward[0]]
    depth = sum((a-b)*f for a,b,f in zip(center,camera['position'],forward))
    tangent = math.tan(math.pi/6)/1.6
    factor = 100*depth*tangent/image['height']
    point = [center[0],center[1]+.65*factor,center[2]+.65*factor]
    relative = [a-b for a,b in zip(point,camera['position'])]
    point_depth = sum(v*f for v,f in zip(relative,forward))
    offset = [image['height']*sum(v*r for v,r in zip(relative,right))/(2*point_depth*tangent),
              -image['height']*sum(v*u for v,u in zip(relative,up))/(2*point_depth*tangent)]
    planar = launch(executable, root, 'plane', (1920,1080),1,start + [
        dict(frame=35,action='mouse',target='viewport.image',down=True,offset=offset),
        dict(frame=38,action='mouse',target='viewport.image',down=True,offset=[offset[0]+30,offset[1]+20]),
        dict(frame=41,action='mouse',target='viewport.image',down=False,offset=[offset[0]+30,offset[1]+20])],
        capture=False,close_policy='discard')
    planar_delta = [a-b for a,b in zip(planar['gizmoTranslation'],center)]
    assert abs(planar_delta[0])<.001 and abs(planar_delta[1])>.001 and abs(planar_delta[2])>.001, 'Plane handle must constrain its normal and move both in-plane axes'
    scaled = launch(executable, root, 'uniform-scale', (1920, 1080), 1, start + [
        dict(frame=33, action='click', target='mode.Scale'),
        dict(frame=36, action='mouse', target='gizmo.center', down=True),
        dict(frame=39, action='mouse', target='gizmo.center', down=True, offset=[40, 0]),
        dict(frame=42, action='mouse', target='gizmo.center', down=False, offset=[40, 0])],
        capture=False, close_policy='discard')
    ratios = [a/b for a,b in zip(scaled['gizmoScale'],baseline['gizmoScale'])]
    assert ratios[0] > 1 and max(ratios)-min(ratios) < .001, 'Center handle applies uniform scale'
    for tool, field, step in [('Rotate', 'gizmoRotation', 15), ('Scale', 'gizmoScale', .1)]:
        result = launch(executable, root, tool.lower() + '-snap', (1920, 1080), 1, start + [
            dict(frame=33, action='click', target='mode.' + tool),
            dict(frame=36, action='mouse', target='gizmo.0', down=True, ctrl=True),
            dict(frame=39, action='mouse', target='gizmo.0', down=True, offset=[60, 40], ctrl=True),
            dict(frame=42, action='mouse', target='gizmo.0', down=False, offset=[60, 40], ctrl=True)],
            capture=False, close_policy='discard')
        value = result[field][0] - baseline[field][0] if tool == 'Rotate' else result[field][0] / baseline[field][0]
        assert abs(value - (0 if tool == 'Rotate' else 1)) > .001, tool + ' snap must change the transform'
        assert abs(value / step - round(value / step)) < .001, tool + ' must obey its own snap step'
        assert result['undoCount'] == baseline['undoCount'] + 1 and not result['capture']['gizmo']
    summary = dict(workflow=workflow, passed=True, commit=True, singleUndo=True, escape=True, focusLoss=True, undo=True, snap=True, rotationSnap=True, scaleSnap=True, uniformScale=True, planar=True)
    (root / 'summary.json').write_text(json.dumps(summary, indent=2), encoding='utf-8')
    return summary


if __name__ == '__main__':
    parser = argparse.ArgumentParser(__doc__)
    parser.add_argument('--executable', type=Path, required=True)
    parser.add_argument('--output', type=Path, required=True)
    args = parser.parse_args()
    root = args.output.resolve() / ('run-' + str(time.time_ns()))
    result = [run(args.executable.resolve(), root / workflow, workflow) for workflow in ('exploration', 'inspector')]
    (root / 'summary.json').write_text(json.dumps(result, indent=2), encoding='utf-8')
    print(json.dumps(result))
