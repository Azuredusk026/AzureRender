"""Validate optional RGB storage precision through public Player materials."""
import argparse
import hashlib
import json
import math
from pathlib import Path
import re
import subprocess
import numpy as np
from PIL import Image
from test_material_radiance_gpu import prepare as prepare_radiance,linear
import test_projected_hair_gpu as hair
ROOT=Path(__file__).resolve().parents[1]
BUDGET=.008

def read(path):return json.loads(path.read_text(encoding='utf-8-sig'))
def save(path,value):path.write_text(json.dumps(value,indent=2)+'\n',encoding='utf-8')
def quantize(values):
    result=np.array(values,dtype=float,copy=True)
    for index in np.ndindex(result.shape):
        value=float(result[index]);mantissa=5 if index[-1]==2 else 6
        maximum=math.ldexp(2-math.ldexp(1,-mantissa),15)
        value=max(0,min(value,maximum));exponent=math.frexp(value)[1]
        quantum=math.ldexp(1,max(exponent-1,-14)-mantissa)
        result[index]=math.floor(value/quantum)*quantum
    return result

def prepare(folder,family,enabled):
    if family=='hair':
        case=next(c for c in hair.cases() if c['name']=='baseline')
        project,oracle=hair.prepare(case,folder);model_path=folder/'assets/panels.gltf';isolation='hair-kk'
        pixels=[r['pixel'] for r in oracle];wanted=np.array([r['linearRgb'] for r in oracle])
    else:
        data=prepare_radiance(folder,'pbr',None,0);project=data['project'];model_path=folder/'plane.gltf';isolation='emissive'
        pixels=data['pixels'];wanted=np.full((len(pixels),3),float(np.float32(.9999)))
    model=read(model_path);active={p.get('material',0) for m in model['meshes'] for p in m['primitives']}
    for index in active:
        material=model['materials'][index];material['alphaMode']='OPAQUE'
        if family=='pbr':material['emissiveFactor']=[.9999]*3
        if enabled:material['extras']['azureRenderMaterial']['features'].append('r11g11b10-output')
    save(model_path,model)
    return project,pixels,quantize(wanted) if enabled else wanted,isolation

def run(player,output,fixed,expected_validation):
    output.mkdir(parents=True,exist_ok=False);program_sha=hashlib.sha256(player.read_bytes()).hexdigest()
    rows=[];images={};summary=dict(status='running',programSha256=program_sha,fixedDescriptors=fixed,maximumLinearErrorBudget=BUDGET,captures=rows)
    try:
        for family in ['pbr','hair']:
            for enabled in [False,True]:
                name=family+('-quantized' if enabled else '-native');folder=output/name
                project,pixels,wanted,isolation=prepare(folder,family,enabled)
                command=[str(player),'--project',str(project),'--width','128','--height','128','--capture-dir',str(folder/'frames'),
                    '--capture-frames','1','--fixed-frame-step','--qa-camera','scene','--qa-light','scene','--qa-isolation',isolation,
                    '--qa-exposure','0','--runtime-report',str(folder/'runtime.json')]
                if fixed:command+=['--disable-bindless','--disable-compute-skinning','--disable-parallel-recording']
                save(folder/'command.json',command);completed=subprocess.run(command,cwd=folder,capture_output=True,text=True,encoding='utf-8',errors='replace',timeout=120)
                log=completed.stdout+completed.stderr;(folder/'player.log').write_text(log,encoding='utf-8')
                assert completed.returncode==0,(name,completed.returncode,log[-2000:])
                assert not re.search(r'VUID-|Validation (?:Error|Warning)|SYNC-HAZARD|\[error\]',log,re.I),name
                messages=[json.loads(line) for line in (folder/'captures/azureplayer.log.jsonl').read_text(encoding='utf-8').splitlines()]
                assert not any(r['level']=='error' for r in messages),name
                assert any(r['message']=='Allocator after unload: buffers=0 images=0' for r in messages),name
                validation='Validation layer: enabled' in log
                if expected_validation:assert validation==(expected_validation=='enabled'),name
                manifest=read(folder/'frames/capture_manifest.json');assert manifest['capturedFrames']==1 and manifest['materialSurfaceProfileLayoutVersion']==22,name
                runtime=read(folder/'runtime.json');assert runtime['scriptErrors']==[] and runtime['presentationErrors']==[],name
                png=folder/'frames/frame_000000.png';image=np.array(Image.open(png).convert('RGB'));images[name]=image
                measured=np.array([linear(image[y,x]) for x,y in pixels]);error=float(abs(measured-wanted).max())
                row=dict(name=name,pixels=pixels,expected=wanted.tolist(),measured=measured.tolist(),maximumLinearError=error,validationEnabled=validation,
                    allocatorZero=True,exitCode=completed.returncode,imageSha256=hashlib.sha256(png.read_bytes()).hexdigest());rows.append(row)
                assert error<=BUDGET,(name,error)
                assert hashlib.sha256(player.read_bytes()).hexdigest()==program_sha,'Program changed during validation'
                print('PASS',name,error,flush=True)
        delta=abs(images['pbr-native'].astype('i4')-images['pbr-quantized'].astype('i4'))
        summary['positiveControlMaximumByteChange']=int(delta.max())
        assert int(delta[:,:,2].max())>=2,'Quantization positive control must change blue by at least two bytes'
        summary['status']='passed'
    except Exception as error:
        summary.update(status='failed',error=repr(error));raise
    finally:save(output/'results.json',summary)

if __name__=='__main__':
    parser=argparse.ArgumentParser(__doc__);parser.add_argument('--executable',type=Path,required=True);parser.add_argument('--output',type=Path,required=True)
    parser.add_argument('--fixed',action='store_true');parser.add_argument('--expected-validation',choices=['enabled','disabled']);args=parser.parse_args()
    run(args.executable.resolve(),args.output.resolve(),args.fixed,args.expected_validation)
