"""Exercise public specular sources through production raster descriptors."""
import argparse
import hashlib
import json
from pathlib import Path
import re
import subprocess
import tempfile
import numpy as np
from PIL import Image
from test_character_probe_gpu import prepare, linear, PROBE_RGB, ROI


def run(player, output, require_validation=False):
    output.mkdir(parents=True, exist_ok=False)
    results, images = [], {}
    cases = [('orm-half', 'metallic-roughness-alpha', .5, 255),
             ('emissive-half', 'emissive-alpha', .5, 64),
             ('orm-zero', 'metallic-roughness-alpha', 0, 255),
             ('legacy', None, 1, 64), ('explicit', 'emissive-alpha', 1, 64)]
    for backend in ('indexed', 'fixed'):
        for isolation in ('reflection', 'direct-specular', 'emissive', 'direct-diffuse'):
            selected = cases if isolation == 'reflection' else cases[:3]
            for name, source, scale, alpha in selected:
                key = f'{backend}-{isolation}-{name}'
                root = output / key
                project = prepare(root, dict(name=key, material='fabric'))
                model_path = root / 'assets/surface.gltf'
                model = json.loads(model_path.read_text())
                mat = model['materials'][0]
                model.setdefault('images', [])
                model.setdefault('textures', [])
                def texture(filename, color):
                    Image.new('RGBA', (1, 1), color).save(root / 'assets' / filename)
                    model['images'].append({'uri': filename})
                    model['textures'].append({'source': len(model['images'])-1})
                    return {'index': len(model['textures'])-1}
                mat['pbrMetallicRoughness']['metallicRoughnessTexture'] = texture('orm.png', (255,255,0,64))
                mat['emissiveTexture'] = texture('emission.png', (70,90,110,alpha))
                mat['emissiveFactor'] = [1,1,1]
                if source:
                    mat['extras']['azureRenderMaterial']['specularInput'] = {
                        'schemaVersion':1, 'source':source, 'scale':scale}
                model_path.write_text(json.dumps(model))
                level_path = root / 'assets/scene.azurelevel'
                level = json.loads(level_path.read_text())
                # A directional positive control exercises both direct layers.
                level['nodes'].append({'id':'light'})
                level['lights']=[{'id':'light','nodeId':'light','intensity':1,'radius':10,
                    'shape':{'type':'directional','direction':[0,0,-1]}}]
                level_path.write_text(json.dumps(level))
                command = [str(player.resolve()), '--project', str(project.resolve()),
                    '--width','128','--height','128','--capture-dir',str(root/'frames'),
                    '--capture-frames','1','--fixed-frame-step','--qa-camera','scene',
                    '--qa-light','scene','--qa-isolation',isolation,
                    '--runtime-report',str(root/'runtime.json')]
                if backend == 'fixed':
                    command += ['--disable-bindless','--disable-compute-skinning','--disable-parallel-recording']
                (root/'command.json').write_text(json.dumps(command))
                with (root/'player.log').open('w') as stream:
                    subprocess.run(command,stdout=stream,stderr=subprocess.STDOUT,check=True,timeout=120)
                log=(root/'player.log').read_text(errors='replace')
                assert not re.search(r'VUID-|Validation (?:Error|Warning)|\[error\]',log),key
                assert f'Character texture access: {backend}' in log,key
                assert 'Allocator after unload: buffers=0 images=0' in log,key
                if require_validation: assert 'Validation layer: enabled' in log,key
                runtime=json.loads((root/'runtime.json').read_text())
                assert not runtime['scriptErrors'] and not runtime['presentationErrors'],key
                image=np.asarray(Image.open(root/'frames/frame_000000.png').convert('RGB'))
                images[key]=image
                error=None
                if isolation=='reflection':
                    wanted=PROBE_RGB*.08*(64/255)*scale
                    error=float(np.abs(linear(image[ROI])-wanted).max())
                results.append(dict(name=key,maximumLinearError=error,center=image[64,64].tolist()))
                (output/'progress.json').write_text(json.dumps(results,indent=2))
                if error is not None: assert error<.004,(key,error)
        for isolation in ('reflection','direct-specular','emissive','direct-diffuse'):
            prefix=f'{backend}-{isolation}-'
            assert np.array_equal(images[prefix+'orm-half'],images[prefix+'emissive-half']),prefix
            if isolation in ('emissive','direct-diffuse'):
                assert np.array_equal(images[prefix+'orm-half'],images[prefix+'orm-zero']),prefix
            else:
                assert np.any(images[prefix+'orm-half']!=images[prefix+'orm-zero']),prefix
            assert images[prefix+'orm-half'].max()>0,prefix
        assert np.array_equal(images[f'{backend}-reflection-legacy'],images[f'{backend}-reflection-explicit'])
    for key in list(images):
        if key.startswith('indexed-'):assert np.array_equal(images[key],images[key.replace('indexed-','fixed-',1)]),key
    summary=dict(status='passed',captures=len(results),cases=results,budget=.004,
        playerSha256=hashlib.sha256(player.read_bytes()).hexdigest(),
        scriptSha256=hashlib.sha256(Path(__file__).read_bytes()).hexdigest())
    (output/'summary.json').write_text(json.dumps(summary,indent=2))
    print(json.dumps(dict(status='passed',captures=len(results))))


if __name__=='__main__':
    p=argparse.ArgumentParser(__doc__)
    p.add_argument('--executable',type=Path,required=True)
    p.add_argument('--output',type=Path)
    p.add_argument('--require-validation',action='store_true')
    a=p.parse_args()
    if a.output:run(a.executable,a.output,a.require_validation)
    else:
        with tempfile.TemporaryDirectory(prefix='specular-input-',dir=Path(__file__).resolve().parents[1]/'build/runs') as root:
            run(a.executable,Path(root)/'captures',a.require_validation)
