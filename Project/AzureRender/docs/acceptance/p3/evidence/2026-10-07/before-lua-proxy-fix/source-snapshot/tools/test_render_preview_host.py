"""Exercise actual auxiliary renderers, output transfers and device cleanup."""
import argparse
import json
import math
import os
from pathlib import Path
import subprocess
import tempfile
from PIL import Image, ImageChops


def run(executable, output):
    output.mkdir(parents=True)
    root = Path(__file__).resolve().parents[1]
    cases = {
        'mixed': [dict(renderer='character', extent=[256,128], position=[2.8,2.1,3.2], target=[0,0,0], transfer='srgb'),
                  dict(renderer='sample', extent=[128,128], position=[2,1,3], target=[0,0,0], transfer='srgb'),
                  dict(renderer='blackhole', extent=[192,128], position=[0,3.5,22], target=[0,0,0], transfer='srgb')],
        'cameras': [dict(renderer='blackhole', extent=[192,128], position=[0,3.5,22], target=[0,0,0], transfer='srgb'),
                    dict(renderer='blackhole', extent=[192,128], position=[-12,8,23], target=[-7,4.5,0], transfer='srgb')],
        'grading': [dict(renderer='character', extent=[192,128], position=[2.8,2.1,3.2], target=[0,0,0], transfer='srgb'),
                    dict(renderer='character', extent=[192,128], position=[2.8,2.1,3.2], target=[0,0,0], transfer='srgb',
                         grade=dict(exposureEv=-3,saturation=0,tint=[.5,1,.5]))],
    }
    evidence = []
    for name, views in cases.items():
        folder=output/name;folder.mkdir()
        descriptor=folder/'views.json';descriptor.write_text(json.dumps(dict(schemaVersion=1,views=views)),encoding='utf-8')
        startup=None
        if os.name=='nt':
            startup=subprocess.STARTUPINFO();startup.dwFlags|=subprocess.STARTF_USESHOWWINDOW;startup.wShowWindow=0
        result=subprocess.run([str(executable),'--asset',str(root/'assets_public/test_model.gltf'),
            '--preview-views',str(descriptor),'--runtime-report',str(folder/'runtime.json'),
            '--width','1280','--height','720','--smoke-frames','24','--fixed-frame-step'],
            cwd=folder,startupinfo=startup,capture_output=True,encoding='utf-8',errors='replace',timeout=180)
        (folder/'stdout.log').write_text(result.stdout,encoding='utf-8')
        (folder/'stderr.log').write_text(result.stderr,encoding='utf-8')
        assert result.returncode==0,result.stdout+result.stderr
        assert 'VUID-' not in result.stderr and 'Validation Error' not in result.stderr,result.stderr
        assert 'Allocator after unload: buffers=0 images=0' in result.stdout,result.stdout
        runtime=json.loads((folder/'runtime.json').read_text(encoding='utf-8'))['developer']
        assert runtime['captures']==len(views)
        assert len({view['colorImage'] for view in runtime['views']})==len(views)
        images=[]
        for index, expected in enumerate(views):
            view=runtime['views'][index]
            assert view['extent']==expected['extent']
            assert all(math.isclose(actual,wanted,abs_tol=1e-5) for actual,wanted in zip(view['cameraPosition'],expected['position']))
            assert view['renderCount']==1,'Static views must suspend after the requested frame'
            image=Image.open(folder/'preview-images'/f'{index}.png').convert('RGB');images.append(image)
            assert list(image.size)==expected['extent']
            if expected['renderer']!='sample':
                assert max(high-low for low,high in image.getextrema())>20,'Actual scene geometry must appear in its independent view'
        if name=='cameras':
            assert ImageChops.difference(images[0],images[1]).getbbox(),'Independent blackhole cameras must change rendered pixels'
        if name=='grading':
            assert ImageChops.difference(images[0],images[1]).getbbox(),'Each view must consume its independent grade settings'
        evidence.append(dict(case=name,views=runtime['views'],released=True,validationErrors=0))
    summary=dict(schemaVersion=1,status='passed',cases=evidence,physicalKeyboardMouse='not performed')
    (output/'summary.json').write_text(json.dumps(summary,indent=2)+'\n',encoding='utf-8');print(json.dumps(summary))


if __name__=='__main__':
    parser=argparse.ArgumentParser(__doc__);parser.add_argument('--executable',type=Path,required=True);parser.add_argument('--output',type=Path)
    args=parser.parse_args()
    if args.output:run(args.executable.resolve(),args.output.resolve())
    else:
        with tempfile.TemporaryDirectory(prefix='azure preview host ') as folder:run(args.executable.resolve(),Path(folder)/'evidence')
