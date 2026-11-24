"""Measure optional visibility output through production rendering and GPU probes."""
import argparse
import hashlib
import json
import math
import os
from pathlib import Path
import re
import subprocess
from PIL import Image

COUNTS=(100,500,10000)
VARIANTS={'cpu':['--disable-gpu-culling'],'default':[],
          'prototype':['--visibility-prototype'],
          'prototype-fixed':['--visibility-prototype','--disable-bindless']}

def validate_scale_report(report):
    rows=report['runs']
    expected={(count,variant) for count in COUNTS for variant in VARIANTS}
    actual=[(row['instances'],row['variant']) for row in rows]
    if len(actual)!=len(expected) or set(actual)!=expected:raise ValueError('Incomplete or duplicate scale matrix')
    for count in COUNTS:
        group=[row for row in rows if row['instances']==count]
        hashes=group[0]['pixelHashes']
        if len(hashes)!=2 or any(row['pixelHashes']!=hashes for row in group):raise ValueError('Production visibility pixels differ')
        for row in group:
            gpu,cpu=row['gpuP95Ms'],row['cpuWorkP95Ms']
            if not math.isfinite(gpu) or not math.isfinite(cpu) or gpu<=0 or cpu<=0:raise ValueError('Invalid timing')
            if gpu>(100 if count==10000 else 16.6) or cpu>(250 if count==10000 else 8):raise ValueError('Experimental scale budget exceeded')
            if not row['released'] or row['validationErrors'] or row['samples']!=120 or row['warmup']!=30:raise ValueError('Lifetime, validation or sampling gate failed')

def main():
    parser=argparse.ArgumentParser(__doc__)
    parser.add_argument('--executable',type=Path,default=Path('build/ninja-msvc-release/AzureRender.exe'))
    parser.add_argument('--probe',type=Path,default=Path('build/ninja-msvc-release/AzureVisibilityGpuTests.exe'))
    parser.add_argument('--shaders',type=Path,default=Path('build/ninja-msvc-release/shaders'))
    parser.add_argument('--output',type=Path,required=True)
    parser.add_argument('--driver',type=Path)
    args=parser.parse_args()
    root=Path(__file__).resolve().parents[1]
    output=args.output.resolve();output.mkdir(parents=True,exist_ok=False)
    env=dict(os.environ,VK_INSTANCE_LAYERS='VK_LAYER_KHRONOS_validation',VK_LAYER_VALIDATE_SYNC='1',VK_LOADER_DEBUG='error,layer')
    if args.driver:env.update(VK_DRIVER_FILES=str(args.driver.resolve()),VK_ICD_FILENAMES=str(args.driver.resolve()))
    startup=subprocess.STARTUPINFO();startup.dwFlags|=subprocess.STARTF_USESHOWWINDOW;startup.wShowWindow=0
    def run(command,folder,name):
        result=subprocess.run(command,cwd=root,env=env,capture_output=True,encoding='utf-8',errors='replace',timeout=600,startupinfo=startup)
        log=result.stdout+result.stderr;(folder/(name+'.log')).write_text(log,encoding='utf-8')
        if result.returncode or any(marker in log.lower() for marker in ['vuid-','sync-hazard','validation error']):raise RuntimeError(f'{name} failed; inspect log')
        return log
    shaders=args.shaders.resolve()
    run([str(args.probe.resolve()),str(shaders/'cull_indirect.comp.spv'),str(shaders/'visibility_prototype.comp.spv'),str(output/'probe.json')],output,'probe')
    probe=json.loads((output/'probe.json').read_text())
    if probe['status']!='passed' or len(probe['runs'])!=6:raise RuntimeError('GPU probe incomplete')
    for row in probe['runs']:
        if row['gpuMeanMs']>2 or row['cpuRecordMeanMs']>2:raise RuntimeError('Standalone compute budget exceeded')
    rows=[]
    for count in COUNTS:
        for variant,flags in VARIANTS.items():
            folder=output/f'{count}-{variant}';folder.mkdir()
            base=[str(args.executable.resolve()),'--asset',str(root/'assets_public/test_model.gltf'),
                '--instances',str(count),'--width','960','--height','540','--fixed-frame-step',*flags]
            log=run([*base,'--capture-dir',str(folder/'capture'),'--capture-frames','2','--capture-fps','60'],folder,'capture')
            if 'Allocator after unload: buffers=0 images=0' not in log:raise RuntimeError('Capture resource leak')
            hashes=[]
            for path in sorted((folder/'capture').glob('frame_*.png')):
                with Image.open(path) as image:hashes.append(hashlib.sha256(image.convert('RGBA').tobytes()).hexdigest())
            log=run([*base,'--smoke-frames','150','--gpu-timing','--gpu-timing-output',str(folder/'timing.json')],folder,'timing')
            if not re.search(r'Insert.*VK_LAYER_KHRONOS_validation',log,re.I):raise RuntimeError('Validation layer not inserted')
            timing=json.loads((folder/'timing.json').read_text())
            # Exclude exactly 30 warmup frames for GPU and CPU.
            import csv
            with (folder/'timing.csv').open() as stream:gpu=[float(row['frameMs']) for row in csv.DictReader(stream)][30:]
            cpu=timing['cpuFrame']['workSamplesMs'][30:]
            if len(gpu)!=120 or len(cpu)!=120:raise RuntimeError('Incomplete timing samples')
            p95=lambda values:sorted(values)[math.ceil(len(values)*.95)-1]
            rows.append(dict(instances=count,variant=variant,pixelHashes=hashes,gpuP95Ms=p95(gpu),cpuWorkP95Ms=p95(cpu),
                samples=len(gpu),warmup=30,released='Allocator after unload: buffers=0 images=0' in log,validationErrors=0,
                submission=timing['submission'],capabilities=json.loads((folder/'capture/gpu_capabilities.json').read_text()),
                access=re.findall(r'Character texture access: (indexed|fixed)',log),
                sceneTriangles=timing['sceneTriangles'],skinnedVertices=timing['skinnedVertices'],transparentTriangles=timing['transparentTriangles']))
            print(f'{count} {variant} measured',flush=True)
    report=dict(schemaVersion=1,status='measured',runs=rows,extent=[960,540],quality='production defaults',
        budgets=dict(standardGpuP95Ms=16.6,standardCpuP95Ms=8,tenThousandGpuP95Ms=100,tenThousandCpuP95Ms=250,
            probeGpuMeanMs=2,probeCpuRecordMeanMs=2),
        scope='frustum visibility and stable surface identities; CPU visibility remains; no Hi-Z occlusion')
    (output/'summary.json').write_text(json.dumps(report,indent=2)+'\n')
    validate_scale_report(report);report['status']='passed'
    (output/'summary.json').write_text(json.dumps(report,indent=2)+'\n')
    return 0

if __name__=='__main__':raise SystemExit(main())
