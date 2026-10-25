"""Exercise raster scene-color refraction through public project and glTF APIs."""
import argparse
import base64
import copy
import io
import json
from pathlib import Path
import re
import subprocess
import tempfile
import numpy as np
from PIL import Image
from create_visibility_fixture import surface


def run(executable, root, quality="existing", expected_validation="enabled"):
    assets = root / 'assets'
    assets.mkdir(parents=True, exist_ok=False)
    surface(assets / 'rear.gltf', double_sided=True)
    source = json.loads((assets / 'rear.gltf').read_text())
    source['materials'][0].update(emissiveFactor=[.6, .15, .05])
    source['materials'][0]['pbrMetallicRoughness']['baseColorFactor'] = [0, 0, 0, 1]
    coplanar=copy.deepcopy(source['materials'][0]);coplanar['emissiveFactor']=[.05,.6,.15]
    source['materials'].append(coplanar)
    for mesh in source['meshes']:
        for primitive in list(mesh['primitives']):
            duplicate=copy.deepcopy(primitive);duplicate['material']=1;mesh['primitives'].append(duplicate)
    (assets / 'rear.gltf').write_text(json.dumps(source))
    surface(assets / 'glass.gltf', double_sided=True, alpha=True)
    glass = json.loads((assets / 'glass.gltf').read_text())
    image = io.BytesIO()
    Image.new('RGBA', (1, 1), (128, 128, 255, 255)).save(image, format='PNG')
    glass['images'] = [{'uri': 'data:image/png;base64,' + base64.b64encode(image.getvalue()).decode()}]
    glass['textures'] = [{'source': 0}]
    material = glass['materials'][0]
    material['pbrMetallicRoughness'].update(baseColorFactor=[0, 0, 0, .75], metallicFactor=0, roughnessFactor=1)
    material['extras'] = {'azureRenderMaterial': {'schemaVersion': 2, 'class': 'generic', 'features': ['scene-refraction'],
        'sceneRefraction': {'texture': 0, 'contribution': .8, 'brightness': 1, 'distortionStrength': 0}}}
    (assets / 'glass.gltf').write_text(json.dumps(glass))
    def component(kind, data):
        return {'type': kind, 'version': 1, 'data': data}
    level = {'schemaVersion': 1, 'id': 'refraction', 'sceneType': 'character', 'lights': [],
        'renderSettings': {'sceneLighting': True, 'skyIntensity': 0, 'skyMode': 2, 'skyColor': [0, 0, 0],
            'platform': False, 'silhouetteOutline': False, 'innerOutline': False, 'stylized': False,
            'bloomEnabled': False, 'antiAliasing': 0, 'exposure': 0, 'contrast': 1},
        'resources': [{'id': 'rear', 'asset': 'assets:/rear.gltf'}, {'id': 'glass', 'asset': 'assets:/glass.gltf'}],
        'nodes': [{'id': 'rear', 'resourceId': 'rear', 'components': {'azure.transform': component('azure.transform', {'translation': [0, 0, -1], 'scale': [4, 4, 4]})}},
            {'id': 'glass', 'resourceId': 'glass', 'components': {'azure.transform': component('azure.transform', {'translation': [0, 0, 0], 'scale': [2, 2, 2]})}},
            {'id': 'camera', 'components': {'azure.transform': component('azure.transform', {'translation': [0, 0, 3]}),
                'azure.camera': component('azure.camera', {'enabled': True, 'priority': 1, 'fieldOfView': 55})}}]}
    project = root / 'project.azureproject'
    project.write_text(json.dumps({'schemaVersion': 1, 'id': 'refraction', 'name': 'Refraction contract',
        'mounts': [{'name': 'assets', 'path': 'assets'}], 'startupScene': 'assets:/scene.azurelevel'}))
    results = {}
    for mode in ['hidden', 'transmit', 'dark', 'fixed']:
        level['nodes'][1]['components']['azure.renderable'] = component('azure.renderable', {'visible': mode != 'hidden'})
        material['extras']['azureRenderMaterial']['sceneRefraction']['brightness'] = 0 if mode == 'dark' else 1
        (assets / 'glass.gltf').write_text(json.dumps(glass))
        (assets / 'scene.azurelevel').write_text(json.dumps(level))
        output = root / mode
        command = [str(executable.resolve()), '--project', str(project.resolve()), '--width', '320', '--height', '240',
            '--capture-dir', str(output.resolve()), '--capture-frames', '1', '--runtime-report', str((root / (mode + '-runtime.json')).resolve())]
        if quality=='multisample':command+=['--set','render.antiAliasing=4']
        if mode == 'fixed':
            command += ['--disable-bindless', '--disable-compute-skinning', '--disable-parallel-recording']
        result = subprocess.run(command, capture_output=True, text=True, encoding='utf-8', errors='replace', timeout=120)
        (root / (mode + '.log')).write_text(result.stdout + result.stderr, encoding='utf-8')
        assert result.returncode == 0 and not re.search(r'VUID-|Validation Error|\[error\]', result.stdout + result.stderr), mode
        assert ('Validation layer: enabled' in result.stdout)==(expected_validation=='enabled'), mode
        manifest=json.loads((output/'capture_manifest.json').read_text())
        if quality=='multisample':assert manifest['rasterizationSamples']==manifest['snapshotRasterizationSamples']==4 and not manifest['multisampleFallback'],mode
        assert 'Allocator after unload: buffers=0 images=0' in result.stdout, mode
        data = json.loads((root / (mode + '-runtime.json')).read_text())
        assert not data['scriptErrors'] and not data['presentationErrors']
        results[mode] = np.asarray(Image.open(output / 'frame_000000.png').convert('RGB'), dtype=np.int16)
    crop = (slice(100, 140), slice(140, 180))
    transmitted = float(np.abs(results['hidden'][crop] - results['transmit'][crop]).mean())
    darkening = float((results['transmit'][crop] - results['dark'][crop]).mean())
    fallback = float(np.abs(results['transmit'] - results['fixed']).mean())
    assert transmitted < 1 and darkening > 10 and fallback < .1, (transmitted, darkening, fallback)
    summary = {'frames': 4, 'qualityMode': quality, 'expectedValidation': expected_validation, 'transmissionDifference': transmitted, 'brightnessResponse': darkening,
        'fixedDescriptorCpuDifference': fallback, 'validationErrors': 0}
    (root / 'summary.json').write_text(json.dumps(summary, indent=2))
    print(json.dumps(summary))


if __name__ == '__main__':
    parser = argparse.ArgumentParser(__doc__)
    parser.add_argument('--executable', required=True, type=Path)
    parser.add_argument('--output', type=Path)
    parser.add_argument('--quality',choices=('existing','multisample'),default='existing')
    parser.add_argument('--expected-validation',choices=('enabled','disabled'),default='enabled')
    args = parser.parse_args()
    if args.output:
        run(args.executable, args.output.resolve(),args.quality,args.expected_validation)
    else:
        runs = Path(__file__).resolve().parents[1] / 'build/runs'
        runs.mkdir(parents=True, exist_ok=True)
        with tempfile.TemporaryDirectory(prefix='scene-refraction-', dir=runs) as temporary:
            run(args.executable, Path(temporary),args.quality,args.expected_validation)
