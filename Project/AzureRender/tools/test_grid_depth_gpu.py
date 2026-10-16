"""Compare grid on/off pixels against independent ray/box occlusion masks."""
import argparse
import json
import os
from pathlib import Path
import subprocess
import numpy as np
from PIL import Image
from create_pathtracing_project import create


def run(executable, root, renderers):
    root.mkdir(parents=True, exist_ok=True)
    results = []
    for renderer in renderers:
        for projection, elevation in (('orthographic', 2), ('perspective', 2), ('orthographic', -2)):
            folder = root / (renderer + '-' + projection + ('-below' if elevation<0 else ''))
            project = create(folder / 'project')
            level = project.parent / 'assets/room.azurelevel'
            scene = json.loads(level.read_text(encoding='utf-8'))
            scene['nodes'] = [n for n in scene['nodes'] if n['id'] == 'diffuse-box']
            scene['nodes'][0]['components']['azure.transform']['data'].update(translation=[0, elevation, 0], scale=[.5, 1, .5])
            scene['renderSettings'].update(renderer=renderer, pathTraceSamples=1, pathTraceDenoise=False, antiAliasing=0)
            scene['lights'] = []
            level.write_text(json.dumps(scene), encoding='utf-8')
            captures = []
            for policy in ('hidden', 'all'):
                work = folder / policy
                work.mkdir()
                actions = [dict(frame=2, command='edit', operation='viewport.orientation', parameters={'axis': '+Y'}),
                           dict(frame=3, command='edit', operation='viewport.projection', parameters={'value': projection}),
                           dict(frame=4, command='edit', operation='viewport.overlay-policy', parameters={'category': 'grid', 'policy': policy})]
                task = work / 'actions.json'
                task.write_text(json.dumps(actions), encoding='utf-8')
                report = work / 'runtime.json'
                command = [str(executable), '--editor-project', str(project), '--editor-close-policy', 'discard',
                           '--width', '1280', '--height', '800', '--fixed-frame-step', '--editor-actions', str(task),
                           '--runtime-report', str(report), '--capture-dir', str(work / 'capture'), '--capture-frames', '16']
                info = subprocess.STARTUPINFO()
                info.dwFlags |= subprocess.STARTF_USESHOWWINDOW
                info.wShowWindow = 0
                result = subprocess.run(command, cwd=work, env=dict(os.environ, AZURERENDER_EDITOR_CONFIG=str(work / 'config')),
                                        capture_output=True, text=True, encoding='utf-8', errors='replace', timeout=90, startupinfo=info)
                log = result.stdout + result.stderr
                (work / 'run.log').write_text(log, encoding='utf-8')
                assert result.returncode == 0, log[-3000:]
                assert 'VUID-' not in log and '[validation error]' not in log, log[-3000:]
                assert 'Allocator after unload: buffers=0 images=0' in log
                data = json.loads(report.read_text(encoding='utf-8'))
                assert all(a['passed'] for a in data['editorActions']), data['editorActions']
                workspace = data['editorWorkspace']
                captures.append(np.asarray(Image.open(work / 'capture/frame_000015.png').convert('RGB')).astype(int))
            view = dict(workspace['camera'], up=[0, 0, -1], fov=.692604)
            distance = np.linalg.norm(np.array(view['target'])-view['position'])
            view['orthoHeight'] = 2*distance*np.tan(view['fov']/2)
            rect = workspace['image']
            yy, xx = np.indices(captures[0].shape[:2])
            u = (xx + .5 - rect['x']) / rect['width']
            v = (yy + .5 - rect['y']) / rect['height']
            forward = np.array(view['target']) - view['position']
            forward /= np.linalg.norm(forward)
            right = np.cross(forward, view['up']); right /= np.linalg.norm(right)
            up = np.cross(right, forward)
            h = view['orthoHeight'] / 2 if projection == 'orthographic' else np.tan(view['fov'] / 2)
            offset = ((2*u-1)*h*rect['width']/rect['height'])[..., None]*right + ((1-2*v)*h)[..., None]*up
            origin = np.broadcast_to(np.array(view['position'], dtype=float), offset.shape)
            direction = np.broadcast_to(forward, offset.shape)
            if projection == 'orthographic':
                origin = origin + offset
            else:
                direction = forward + offset
                direction /= np.linalg.norm(direction, axis=-1)[..., None]
            with np.errstate(divide='ignore', invalid='ignore'):
                # Shrink the box to avoid silhouette filtering and jitter.
                a = (np.array([-.45, elevation-.9, -.45])-origin)/direction
                b = (np.array([.45, elevation+.9, .45])-origin)/direction
                near = np.minimum(a, b).max(axis=-1)
                far = np.maximum(a, b).min(axis=-1)
                ground = -origin[..., 1]/direction[..., 1]
            inside = (u>.05)&(u<.95)&(v>.08)&(v<.92)
            box = inside & (near>0) & (far>=near)
            blocked = box & (ground>far)
            in_front = box & (ground>0) & (ground<near)
            exposed = inside & (far<near) & (ground>0)
            delta = np.max(np.abs(captures[1]-captures[0]), axis=-1)
            blocked_changes = int(np.count_nonzero((delta>3)&blocked))
            exposed_changes = int(np.count_nonzero((delta>3)&exposed))
            front_changes = int(np.count_nonzero((delta>3)&in_front))
            record = dict(renderer=renderer, projection=projection, elevation=elevation, blockedPixels=int(blocked.sum()),
                          blockedChanges=blocked_changes, visibleGridPixels=exposed_changes, gridInFrontPixels=front_changes)
            results.append(record)
            (root/'summary.json').write_text(json.dumps(results, indent=2), encoding='utf-8')
            print(record, flush=True)
            assert box.sum()>500, record
            assert blocked_changes == 0, 'Ground grid leaks over the foreground box: '+str(record)
            assert exposed_changes>100, 'Exposed ground grid missing: '+str(record)
            if elevation<0:
                assert front_changes>30, 'Grid in front of underground geometry must stay visible: '+str(record)


if __name__ == '__main__':
    parser = argparse.ArgumentParser(__doc__)
    parser.add_argument('--executable', type=Path, required=True)
    parser.add_argument('--output', type=Path, required=True)
    parser.add_argument('--renderers', nargs='+', default=['character', 'pathtracing'])
    args = parser.parse_args()
    run(args.executable.resolve(), args.output.resolve(), args.renderers)
