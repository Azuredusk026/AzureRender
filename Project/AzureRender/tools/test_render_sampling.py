"""Verify renderer sampling against an independently rendered high-resolution reference."""
import argparse,json,subprocess,time
from pathlib import Path
import numpy as np
from PIL import Image

def linear(image):
    x=np.asarray(Image.open(image).convert('RGB'),dtype=np.float32)/255
    return np.where(x<=.04045,x/12.92,((x+.055)/1.055)**2.4)

def main():
    p=argparse.ArgumentParser(__doc__)
    p.add_argument('--executable',type=Path,required=True);p.add_argument('--asset',type=Path,required=True);p.add_argument('--output',type=Path,required=True)
    args=p.parse_args();root=args.output.resolve()/('run-'+str(time.time_ns()));root.mkdir(parents=True,exist_ok=True)
    images={};manifests={}
    for name,quality,width,height in [('off','off',640,360),('edge','edge',640,360),('ss','supersample',640,360),('reference','off',1280,720)]:
        output=root/name
        command=[str(args.executable.resolve()),'--asset',str(args.asset.resolve()),'--width',str(width),'--height',str(height),'--qa-camera','full-body-front',
                 '--qa-effect','outline','--qa-effect-state','disabled','--anti-aliasing',quality,'--capture-frames','1','--capture-dir',str(output),'--gpu-timing']
        run=subprocess.run(command,capture_output=True,text=True,encoding='utf-8',errors='replace',timeout=240)
        log=run.stdout+run.stderr;(root/(name+'.log')).write_text(log,encoding='utf-8')
        assert run.returncode==0 and 'VUID-' not in log and 'Validation Error' not in log,log[-4000:]
        assert 'Allocator after unload: buffers=0 images=0' in log
        images[name]=linear(output/'frame_000000.png');manifests[name]=json.loads((output/'capture_manifest.json').read_text(encoding='utf-8'))
        print(name,'captured',flush=True)
    high=images['reference'];reference=high.reshape(360,2,640,2,3).mean((1,3))
    errors={name:float(np.mean(np.abs(image-reference))) for name,image in images.items() if name!='reference'}
    report={'linearMeanAbsoluteError':errors,'outputExtent':[640,360],'supersampleSceneExtent':[manifests['ss']['sceneRenderWidth'],manifests['ss']['sceneRenderHeight']]}
    report['captureDirectory']=str(root)
    (root/'summary.json').write_text(json.dumps(report,indent=2),encoding='utf-8')
    (root.parent/'summary.json').write_text(json.dumps(report,indent=2),encoding='utf-8');print(report)
    assert report['supersampleSceneExtent']==[1280,720]
    assert errors['ss']<.0015,'Four subpixels must resolve in linear display space'
    assert errors['ss']<errors['off']*.5,'Supersampling must approach the independent reference'
    assert errors['edge']<errors['off'],'Edge filtering must reduce raster error'
if __name__=='__main__':main()
