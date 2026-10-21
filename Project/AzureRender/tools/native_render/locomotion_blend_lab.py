"""Verify speed-driven weighted poses and independent actors in the real Player."""
import argparse
import base64
import copy
import hashlib
import json
import math
from pathlib import Path
import re
import struct
import subprocess
import sys

sys.path.insert(0, str(Path(__file__).resolve().parents[1]))
from create_third_person_project import create


def add_fast_clip(path):
    document = json.loads(path.read_text(encoding='utf-8'))
    action = copy.deepcopy(document['animations'][1])
    action['name'] = 'fixture-fast-gait'
    mapped = {}
    for sampler in action['samplers']:
        index = sampler['input']
        if index not in mapped:
            accessor = copy.deepcopy(document['accessors'][index])
            view = document['bufferViews'][accessor['bufferView']]
            blob = base64.b64decode(document['buffers'][view['buffer']]['uri'].split(',', 1)[1])
            assert accessor['componentType'] == 5126 and accessor['type'] == 'SCALAR'
            start = view.get('byteOffset', 0) + accessor.get('byteOffset', 0)
            times = [struct.unpack_from('<f', blob, start+i*view.get('byteStride', 4))[0]*.5
                     for i in range(accessor['count'])]
            packed = struct.pack('<'+'f'*len(times), *times)
            buffer = len(document['buffers'])
            document['buffers'].append({'byteLength': len(packed),
                'uri': 'data:application/octet-stream;base64,'+base64.b64encode(packed).decode()})
            accessor['bufferView'] = len(document['bufferViews'])
            accessor.update(byteOffset=0, min=[min(times)], max=[max(times)])
            document['bufferViews'].append({'buffer': buffer, 'byteLength': len(packed)})
            mapped[index] = len(document['accessors'])
            document['accessors'].append(accessor)
        sampler['input'] = mapped[index]
    document['animations'].append(action)
    durations = []
    for animation in document['animations']:
        times = [document['accessors'][s['input']] for s in animation['samplers']]
        durations.append(max(a['max'][0] for a in times)-min(a['min'][0] for a in times))
    path.write_text(json.dumps(document), encoding='utf-8')
    return durations


def run(player, output):
    output.mkdir(parents=True, exist_ok=False)
    project = output/'project'
    create(project)
    durations = add_fast_clip(project/'assets/hero.gltf')
    assert add_fast_clip(project/'assets/guide.gltf') == durations
    graph = json.loads((project/'assets/locomotion.json').read_text())
    graph.update(schemaVersion=2, locomotion={'schemaVersion': 1, 'smoothingSeconds': .12,
        'maximumCyclesPerSecond': 6, 'samples': [
            {'clip': i, 'speed': speed, 'duration': durations[i]} for i,speed in enumerate((0,2,5))]})
    (project/'assets/locomotion.json').write_text(json.dumps(graph, indent=2))
    path = project/'assets/courtyard.azurelevel'
    level = json.loads(path.read_text())
    level['nodes'] = [n for n in level['nodes'] if n['id'] in ('hero','guide','ground','camera')]
    for node in level['nodes']:
        if node['id'] == 'ground':
            node['components']['azure.transform']['data'].update(translation=[0,-.25,0], scale=[50,.25,50])
        if node['id'] == 'guide':
            node['components']['azure.character'] = {'type': 'azure.character', 'version': 4,
                                                     'data': {'controlled': False}}
            node['components']['azure.animator']['data']['locomotion'] = True
    path.write_text(json.dumps(level, indent=2))
    events = [(30,87,True),(100,340,True),(190,68,True),(240,340,False),
              (300,87,False),(300,68,False)]
    (output/'input.json').write_text(json.dumps({'schemaVersion':1,'actions':[
        {'frame': f, 'action': 'key', 'key': key, 'down': down} for f,key,down in events]}))
    command = [str(player), '--project', str(project/'project.azureproject'), '--width', '640', '--height', '360',
        '--capture-dir', str(output/'frames'), '--capture-frames', '360', '--capture-fps', '60',
        '--fixed-frame-step', '--game-actions', str(output/'input.json'), '--runtime-report', str(output/'runtime.json')]
    (output/'command.json').write_text(json.dumps(command, indent=2))
    startup = subprocess.STARTUPINFO()
    startup.dwFlags |= subprocess.STARTF_USESHOWWINDOW
    startup.wShowWindow = 0
    with (output/'player.log').open('w') as log:
        subprocess.run(command, stdout=log, stderr=subprocess.STDOUT, check=True, timeout=300, startupinfo=startup)
    log = (output/'player.log').read_text(errors='replace')
    assert not re.search(r'VUID-|Validation Error|\[error\]', log)
    assert 'Allocator after unload: buffers=0 images=0' in log
    data = json.loads((output/'runtime.json').read_text())
    assert not data['scriptErrors'] and not data['presentationErrors']
    maximum_weight_error = 0
    maximum_phase_error = 0
    counts = {}
    three_way = 0
    for frame in data['routeFrames']:
        animations = {a['node']: a for a in frame['animations']}
        if not animations:
            continue
        hero,guide = animations['hero'],animations['guide']
        for actor in (hero,guide):
            samples = actor['samples']
            assert samples and all(math.isfinite(s['time']) and s['weight'] >= 0 for s in samples)
            maximum_weight_error = max(maximum_weight_error, abs(sum(s['weight'] for s in samples)-1))
            gaits = [s for s in samples if s['clip'] != 0]
            if len(gaits) > 1:
                maximum_phase_error = max(maximum_phase_error,
                    abs(gaits[0]['time']/gaits[0]['duration']-gaits[1]['time']/gaits[1]['duration']))
        assert guide['samples'] == [{'clip':0,'time':guide['samples'][0]['time'],
                                     'weight':1.,'loop':True,'duration':durations[0]}]
        counts[hero['clip']] = counts.get(hero['clip'], 0)+1
        three_way += len(hero['samples']) == 3
    assert set(counts) == {0,1,2} and three_way > 0
    assert maximum_weight_error <= 1e-5 and maximum_phase_error <= 1e-5
    last = next(a for a in data['routeFrames'][-1]['animations'] if a['node'] == 'hero')
    assert next(s['weight'] for s in last['samples'] if s['clip'] == 0) > .99
    result = dict(playerSha256=hashlib.sha256(player.read_bytes()).hexdigest(),
        maximumWeightError=maximum_weight_error, maximumPhaseError=maximum_phase_error,
        dominantClipFrames=counts, threeWayFrames=three_way, independentActorVerified=True,
        resourcesAfterUnload=0, frames=len(data['routeFrames']))
    (output/'metrics.json').write_text(json.dumps(result, indent=2))
    print(json.dumps(result))


if __name__ == '__main__':
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--player', type=Path, required=True)
    parser.add_argument('--output', type=Path)
    args = parser.parse_args()
    managed = None
    cleanup = Path(__file__).resolve().parents[1]/'artifact_cleanup.py'
    if args.output is None:
        managed = Path(subprocess.check_output([sys.executable, str(cleanup), '--new-run',
            'locomotion-blend-lab'], text=True).strip())
    run(args.player.resolve(), args.output.resolve() if args.output else managed/'lab')
    if managed:
        subprocess.run([sys.executable, str(cleanup), '--finish', str(managed)], check=True)
