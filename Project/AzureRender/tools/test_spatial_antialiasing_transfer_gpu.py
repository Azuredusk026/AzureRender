"""Verify actual UNORM auxiliary outputs against a known linear-color oracle."""
import argparse
import hashlib
import json
from pathlib import Path
import re
import subprocess
import tempfile
import numpy as np
from PIL import Image


def run(executable,output):
    output.mkdir(parents=True,exist_ok=True);cases=[]
    for transfer in ('linear','srgb'):
        folder=output/transfer;folder.mkdir()
        views=[{'renderer':'sample','extent':[96,64],'position':[0,1,3],'target':[0,1,0],
            'antiAliasing':mode,'transfer':transfer,
            'grade':{'exposureEv':0,'saturation':1,'contrast':0,'tint':[1,1,1],'toneMappingEnabled':False}} for mode in range(3)]
        # A zero contrast produces exactly .5 linear after every source value.
        descriptor=folder/'views.json';descriptor.write_text(json.dumps({'schemaVersion':1,'views':views}))
        command=[str(executable.resolve()),'--asset',str(Path(__file__).resolve().parents[1]/'assets_public/test_model.gltf'),
            '--preview-views',str(descriptor),'--runtime-report',str(folder/'runtime.json'),'--smoke-frames','4',
            '--fixed-frame-step','--width','320','--height','180']
        result=subprocess.run(command,cwd=folder,capture_output=True,text=True,encoding='utf8',errors='replace',timeout=120)
        log=result.stdout+result.stderr;(folder/'run.log').write_text(log,encoding='utf8')
        assert result.returncode==0 and not re.search(r'VUID-|Validation Error|\[error\]',log),log[-3000:]
        assert 'Allocator after unload: buffers=0 images=0' in log
        runtime=json.loads((folder/'runtime.json').read_text())['developer']
        assert runtime['captures']==3
        expected=128 if transfer=='linear' else round((1.055*.5**(1/2.4)-.055)*255)
        for mode,view in enumerate(runtime['views']):
            frame=np.asarray(Image.open(folder/'preview-images'/f'{mode}.png').convert('RGB'),dtype=float)
            error=float(abs(frame-expected).max())
            assert error<=1,(transfer,mode,error)
            assert view['antiAliasing']==mode
            assert view['sceneExtent']==([192,128] if mode==2 else [96,64])
            assert view['outputFormat']==('rgba8-linear' if transfer=='linear' else 'rgba8-srgb-encoded')
            cases.append(dict(transfer=transfer,mode=mode,expectedByte=expected,maximumByteError=error,sceneExtent=view['sceneExtent']))
    summary=dict(schemaVersion=1,executableSha256=hashlib.sha256(executable.read_bytes()).hexdigest(),
        oracle='contrast=0 produces exactly .5 linear; CPU IEC sRGB encodes .5 to 188',cases=cases,
        validationErrorsDetected=0,liveImagesAfterUnload=0)
    (output/'summary.json').write_text(json.dumps(summary,indent=2)+'\n');print(json.dumps(summary))


if __name__=='__main__':
    parser=argparse.ArgumentParser(__doc__);parser.add_argument('--executable',required=True,type=Path);parser.add_argument('--output',type=Path)
    args=parser.parse_args()
    if args.output:run(args.executable,args.output.resolve())
    else:
        with tempfile.TemporaryDirectory(prefix='spatial-transfer-',dir=Path(__file__).resolve().parents[1]/'build/runs') as output:
            run(args.executable,Path(output))
