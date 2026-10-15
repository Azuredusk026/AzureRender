"""Capture actual Player output at controlled sample counts without frame dumps."""
import argparse,json,subprocess,time
from pathlib import Path
def run(executable,project,output,samples=(64,256,1024),seed=0,size=(640,480),denoise=False):
    project=Path(project).resolve();output=Path(output).resolve();output.mkdir(parents=True,exist_ok=True)
    if len(samples)>1:
        records=[run(executable,project,output/str(sample),(sample,),seed,size,denoise) for sample in samples]
        (output/'summary.json').write_text(json.dumps(records,indent=2),encoding='utf-8');return records
    data=json.loads(project.read_text(encoding='utf-8'));level=project.parent/'assets/room.azurelevel';original=level.read_bytes();scene=json.loads(original)
    scene['renderSettings'].update(pathTraceSamples=max(samples),pathTraceSeed=seed,pathTraceDenoise=denoise)
    steps=[];frame=0
    for sample in samples:
        target=sample+3
        remaining=target-frame
        while remaining>0:
            count=min(32,remaining);steps.append({'op':'wait-frames','frames':count,'timeoutMs':60000});remaining-=count
        steps.append({'op':'screenshot','label':f'samples-{sample}'});frame=target+1
    steps.append({'op':'wait-frames','frames':3});script=output/'steps.json';script.write_text(json.dumps({'schemaVersion':1,'steps':steps}),encoding='utf-8')
    command=[str(Path(executable).resolve()),'--project',str(project),'--width',str(size[0]),'--height',str(size[1]),'--resource-root',str(output),'--validation-script',str(script),'--validation-report',str(output/'validation.json'),'--runtime-report',str(output/'runtime.json'),'--smoke-frames',str(max(samples)+128)]
    start=time.perf_counter()
    try:
        level.write_text(json.dumps(scene),encoding='utf-8')
        result=subprocess.run(command,cwd=output,stdout=subprocess.PIPE,stderr=subprocess.STDOUT,encoding='utf-8',errors='replace',timeout=1200)
    finally:level.write_bytes(original)
    (output/'run.log').write_text(result.stdout,encoding='utf-8')
    assert result.returncode==0,result.stdout[-4000:]
    assert '[validation error]' not in result.stdout and 'VUID-' not in result.stdout,result.stdout[-4000:]
    assert 'Allocator after unload: buffers=0 images=0' in result.stdout,result.stdout[-2000:]
    assert json.loads((output/'validation.json').read_text(encoding='utf-8'))['passed']
    for sample in samples:assert (output/f'captures/samples-{sample}.png').is_file()
    record={'project':str(project),'samples':list(samples),'seed':seed,'size':size,'denoise':denoise,'seconds':time.perf_counter()-start,'validationEnabled':'Validation layer: enabled' in result.stdout,'released':True}
    (output/'summary.json').write_text(json.dumps(record,indent=2),encoding='utf-8');print(json.dumps(record),flush=True);return record
if __name__=='__main__':
    p=argparse.ArgumentParser(__doc__);p.add_argument('--executable',required=True);p.add_argument('--project',required=True);p.add_argument('--output',required=True);p.add_argument('--samples',nargs='+',type=int,default=[64,256,1024]);p.add_argument('--seed',type=int,default=0);p.add_argument('--width',type=int,default=640);p.add_argument('--height',type=int,default=480);p.add_argument('--denoise',action='store_true');a=p.parse_args();run(a.executable,a.project,a.output,a.samples,a.seed,(a.width,a.height),a.denoise)
