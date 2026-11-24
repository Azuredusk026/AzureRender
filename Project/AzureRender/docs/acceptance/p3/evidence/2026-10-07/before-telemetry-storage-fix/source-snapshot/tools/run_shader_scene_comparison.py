"""Compare composed Bloom programs through the production RenderGraph host."""
import argparse
import hashlib
import json
import os
from pathlib import Path
import re
import shutil
import subprocess
import time
from PIL import Image

ROOT=Path(__file__).resolve().parents[1]


def main():
    parser=argparse.ArgumentParser(__doc__)
    parser.add_argument('--executable',type=Path,required=True)
    parser.add_argument('--shaders',type=Path,required=True)
    parser.add_argument('--modules',type=Path,required=True)
    parser.add_argument('--output',type=Path,required=True)
    parser.add_argument('--driver',type=Path)
    args=parser.parse_args()
    output=args.output.resolve()
    output.mkdir(parents=True,exist_ok=False)
    env=dict(os.environ,VK_INSTANCE_LAYERS='VK_LAYER_KHRONOS_validation',
        VK_LAYER_VALIDATE_SYNC='1',VK_LOADER_DEBUG='error,layer')
    if args.driver:
        env.update(VK_DRIVER_FILES=str(args.driver.resolve()),VK_ICD_FILENAMES=str(args.driver.resolve()))
    rows=[]
    for name in ['glsl','direct','cached']:
        folder=output/name;folder.mkdir()
        shutil.copytree(args.shaders,folder/'shaders')
        if name!='glsl':
            shutil.copy2(args.modules/f'bloom-{name}.spv',folder/'shaders/bloom_downsample.comp.spv')
        base=[str(args.executable.resolve()),'--resource-root',str(folder),'--scene-type','blackhole',
            '--blackhole-quality','balanced','--blackhole-camera','front','--width','960','--height','540']
        hashes=[]
        for kind,options in [('capture',['--capture-dir',str(folder/'capture'),'--capture-frames','4','--capture-fps','60']),
                             ('timing',['--fixed-frame-step','--smoke-frames','120','--gpu-timing','--gpu-timing-output',str(folder/'timing.json')])]:
            start=time.monotonic()
            startup=subprocess.STARTUPINFO();startup.dwFlags|=subprocess.STARTF_USESHOWWINDOW;startup.wShowWindow=0
            result=subprocess.run([*base,*options],cwd=folder,env=env,capture_output=True,text=True,
                encoding='utf-8',errors='replace',timeout=240,startupinfo=startup)
            log=result.stdout+result.stderr
            (folder/(kind+'.log')).write_text(log,encoding='utf-8')
            if result.returncode or any(marker in log.lower() for marker in ['vuid-','sync-hazard','validation error']):
                raise RuntimeError(f'{name}/{kind} failed; inspect log')
            if not re.search(r'Insert.*VK_LAYER_KHRONOS_validation',log,re.I):
                raise RuntimeError('Real validation-layer insertion was not observed')
            if 'Allocator after unload: buffers=0 images=0' not in log:
                raise RuntimeError('Production renderer failed to release allocations')
            if kind=='timing':
                seconds=time.monotonic()-start
        frames=sorted((folder/'capture').glob('frame_*.png'))
        if len(frames)!=4: raise RuntimeError('Incomplete frozen scene capture')
        for frame in frames:
            with Image.open(frame) as image:
                hashes.append(hashlib.sha256(image.convert('RGBA').tobytes()).hexdigest())
        timing=json.loads((folder/'timing.json').read_text(encoding='utf-8'))
        if timing['samples']!=120: raise RuntimeError('Incomplete production timing')
        rows.append(dict(name=name,pixelHashes=hashes,gpuAverageMs=timing['totalAverageMs'],
            gpuP95Ms=timing['totalP95Ms'],processSeconds=seconds,validationLayerInserted=True,released=True,
            shaderHash=hashlib.sha256((folder/'shaders/bloom_downsample.comp.spv').read_bytes()).hexdigest()))
        if hashes!=rows[0]['pixelHashes']: raise RuntimeError('Composed production scene pixels differ')
        print(name+' production pixels and validation passed',flush=True)
    report=dict(schemaVersion=1,status='passed',variants=rows,extent=[960,540],quality='balanced',
        camera='front',captureFrames=4,timingFrames=120,
        driverHash=hashlib.sha256(args.driver.read_bytes()).hexdigest() if args.driver else None)
    (output/'summary.json').write_text(json.dumps(report,indent=2)+'\n',encoding='utf-8')


if __name__=='__main__':main()
