"""Compare real Player compute and vertex consumers using a third morph target."""
import argparse
import base64
import json
from pathlib import Path
import struct
import subprocess
import numpy as np
from PIL import Image
from test_independent_animation_gpu import component


def fixture(source, destination):
    data = json.loads(source.read_text(encoding='utf-8'))
    binary = bytearray(base64.b64decode(data['buffers'][0]['uri'].split(',')[1]))
    for mesh in data['meshes']:
        mesh['weights'] = [0, 0, 0]
        for primitive in mesh['primitives']:
            count = data['accessors'][primitive['attributes']['POSITION']]['count']
            targets = []
            for target in range(3):
                attributes = {}
                for semantic, delta in [('POSITION', (.15, .07, 0)), ('NORMAL', (.25, 0, 0)), ('TANGENT', (0, .25, 0))]:
                    while len(binary) % 4:
                        binary.append(0)
                    payload = struct.pack('<3f', *(delta if target == 2 else (0, 0, 0))) * count
                    view = len(data['bufferViews'])
                    data['bufferViews'].append({'buffer': 0, 'byteOffset': len(binary), 'byteLength': len(payload)})
                    binary.extend(payload)
                    attributes[semantic] = len(data['accessors'])
                    data['accessors'].append({'bufferView': view, 'componentType': 5126, 'count': count, 'type': 'VEC3'})
                targets.append(attributes)
            primitive['targets'] = targets
    data['buffers'][0] = {'byteLength': len(binary), 'uri': 'data:application/octet-stream;base64,' + base64.b64encode(binary).decode()}
    destination.write_text(json.dumps(data), encoding='utf-8')


def capture(executable, root, name, fallback=False, zero=False, outline=True):
    project = root / name
    assets = project / 'assets'
    assets.mkdir(parents=True)
    fixture(Path(__file__).resolve().parents[1] / 'assets_public/third_person/explorer.gltf', assets / 'hero.gltf')
    (assets / 'motion.json').write_text(json.dumps({'schemaVersion': 1, 'initial': 'idle', 'states': [{'name': 'idle', 'clip': 0}], 'transitions': []}), encoding='utf-8')
    nodes = []
    for i, weight in enumerate([.8, -.6]):
        nodes.append({'id': f'hero-{i}', 'resourceId': 'hero', 'components': {
            'azure.transform': component('azure.transform', {'translation': [(i * 2 - 1) * .6, 0, 0]}),
            'azure.animator': component('azure.animator', {'asset': 'assets:/motion.json', 'state': 'idle', 'startTime': i * .2, 'morphOverrides': json.dumps({'schemaVersion': 1, 'weights': [0, 0, 0 if zero else weight]})}, 4)}})
    (assets / 'scene.azurelevel').write_text(json.dumps({'schemaVersion': 1, 'id': 'third-target', 'sceneType': 'character', 'resources': [{'id': 'hero', 'asset': 'assets:/hero.gltf'}], 'nodes': nodes}), encoding='utf-8')
    (project / 'project.azureproject').write_text(json.dumps({'schemaVersion': 1, 'id': name, 'name': name, 'mounts': [{'name': 'assets', 'path': 'assets'}], 'startupScene': 'assets:/scene.azurelevel'}), encoding='utf-8')
    output = project / 'frames'
    report = project / 'runtime.json'
    command = [str(executable), '--project', str(project / 'project.azureproject'), '--width', '960', '--height', '540', '--capture-dir', str(output), '--capture-frames', '4', '--capture-fps', '4', '--runtime-report', str(report), '--qa-camera', 'full-body-front', '--qa-light', 'stylized-key', '--qa-animation']
    if fallback:
        command += ['--disable-compute-skinning', '--disable-bindless', '--disable-parallel-recording']
    if not outline:
        command += ['--qa-effect', 'outline', '--qa-effect-state', 'disabled']
    startup = subprocess.STARTUPINFO()
    startup.dwFlags |= subprocess.STARTF_USESHOWWINDOW
    result = subprocess.run(command, capture_output=True, encoding='utf-8', errors='replace', timeout=120, startupinfo=startup)
    (project / 'stdout.log').write_text(result.stdout, encoding='utf-8')
    (project / 'stderr.log').write_text(result.stderr, encoding='utf-8')
    assert result.returncode == 0, result.stdout + result.stderr
    assert 'VUID-' not in result.stderr and 'Validation Error' not in result.stderr, result.stderr
    assert 'Allocator after unload: buffers=0 images=0' in result.stdout
    assert not json.loads(report.read_text(encoding='utf-8'))['presentationErrors']
    frames = [np.asarray(Image.open(path).convert('RGB'), dtype=np.int16) for path in sorted(output.glob('frame_*.png'))]
    assert len(frames) == 4, len(frames)
    return frames


def main():
    parser = argparse.ArgumentParser(__doc__)
    parser.add_argument('--executable', type=Path, required=True)
    parser.add_argument('--output', type=Path, required=True)
    args = parser.parse_args()
    root = args.output.resolve()
    compute = capture(args.executable.resolve(), root, 'compute')
    vertex = capture(args.executable.resolve(), root, 'vertex', fallback=True)
    neutral = capture(args.executable.resolve(), root, 'neutral', zero=True)
    plain = capture(args.executable.resolve(), root, 'no-outline', outline=False)
    differences = [float(np.abs(a - b).mean()) for a, b in zip(compute, vertex)]
    assert max(differences) < .1, differences
    changed = int(np.count_nonzero(np.max(np.abs(compute[1] - neutral[1]), axis=2) > 3))
    outlined = int(np.count_nonzero(np.max(np.abs(compute[1] - plain[1]), axis=2) > 3))
    assert changed > 100, changed
    assert outlined > 20, outlined
    summary = {'instances': 2, 'targets': 3, 'independentWeights': [.8, -.6], 'computeVertexMeanDifference': differences, 'thirdTargetChangedPixels': changed, 'outlineChangedPixels': outlined, 'validationErrors': 0}
    (root / 'summary.json').write_text(json.dumps(summary, indent=2) + '\n', encoding='utf-8')
    print(json.dumps(summary))


if __name__ == '__main__':
    main()
