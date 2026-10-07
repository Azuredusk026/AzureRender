"""Compare actual editor previews enabled and disabled on two public workflows."""
import argparse
import csv
import hashlib
import json
import os
from pathlib import Path
import shutil
import subprocess
import time
from create_playable_project import create
from run_playable_performance import memory, stats, thermal
from build_provenance import inputs


def run(executable, project, folder, enabled, samples, warmup, cooling):
    folder.mkdir(parents=True)
    actions = [dict(frame=20,action='click',target='assets.grid'),
               dict(frame=35,action='click',target='tool.capture'),
               dict(frame=50,action='click',target='preview.camera')] if enabled else []
    path=folder/'ui.json';path.write_text(json.dumps(actions),encoding='utf-8')
    env=dict(os.environ,AZURERENDER_EDITOR_CONFIG=str(folder/'config'),AZURERENDER_EDITOR_UI_ACTIONS=str(path))
    if cooling:
        while thermal()['temperature']>55:
            print('Waiting for GPU cooling before preview sampling',flush=True);time.sleep(10)
    start=time.monotonic();peak=0
    startup=subprocess.STARTUPINFO();startup.dwFlags|=subprocess.STARTF_USESHOWWINDOW;startup.wShowWindow=0
    with (folder/'stdout.log').open('w',encoding='utf-8') as stdout,(folder/'stderr.log').open('w',encoding='utf-8') as stderr:
        process=subprocess.Popen([str(executable),'--editor-project',str(project), '--width','1920','--height','1080',
            '--fixed-frame-step','--smoke-frames',str(warmup+samples),'--runtime-report',str(folder/'runtime.json'),
            '--gpu-timing','--gpu-timing-output',str(folder/'timing.json')],env=env,cwd=folder,stdout=stdout,stderr=stderr,startupinfo=startup)
        while process.poll() is None:
            peak=max(peak,memory(process.pid))
            if time.monotonic()-start>600:process.kill();process.wait();raise TimeoutError('Preview comparison exceeded deadline')
            time.sleep(.05)
    logs=(folder/'stdout.log').read_text(encoding='utf-8')+(folder/'stderr.log').read_text(encoding='utf-8')
    assert process.returncode==0,logs[-4000:]
    assert not any(word in logs.lower() for word in ('vuid-','validation error','hazard detected')),logs[-4000:]
    assert 'Allocator after unload: buffers=0 images=0' in logs
    data=json.loads((folder/'timing.json').read_text(encoding='utf-8'))
    runtime=json.loads((folder/'runtime.json').read_text(encoding='utf-8'))
    assert not runtime['editorWorkspace']['uiErrors'],runtime['editorWorkspace']['uiErrors']
    previews=runtime['developer']
    if enabled:
        assert previews['camera'] and previews['camera']['renderCount']>samples
        assert previews['thumbnails'] and len(previews['thumbnails'])<=2
        assert all(entry['view']['renderCount']==1 for entry in previews['thumbnails'])
    else: assert previews['camera'] is None and not previews['thumbnails']
    with (folder/'timing.csv').open(encoding='utf-8') as stream:gpu=[float(row['frameMs']) for row in csv.DictReader(stream)][warmup:]
    assert len(gpu)==samples
    result=dict(gpu=stats(gpu),cpu=stats(data['cpuFrame']['workSamplesMs'][warmup:]),peakWorkingSetBytes=peak,
                deviceLocalPeakBytes=data['allocator']['deviceLocalPeakBytes'],samples=samples,warmup=warmup,
                elapsedSeconds=time.monotonic()-start,previewEnabled=enabled,developer=previews,
                settings=dict(window=[1920,1080],cameraExtent=[320,180],thumbnailExtent=[128,128],capacity=3))
    result['passed']=result['gpu']['p95']<=16.6 and result['cpu']['p95']<=10 and peak<=2*1024**3 and result['deviceLocalPeakBytes']<=1.5*1024**3
    print(folder.name,json.dumps({name:result[name] for name in ('gpu','cpu','passed')}),flush=True)
    return result


def main():
    parser=argparse.ArgumentParser(__doc__)
    parser.add_argument('--executable',required=True,type=Path);parser.add_argument('--output',required=True,type=Path)
    parser.add_argument('--samples',type=int,default=1800);parser.add_argument('--warmup',type=int,default=300)
    parser.add_argument('--rounds',type=int,default=3);parser.add_argument('--skip-cooling',action='store_true')
    args=parser.parse_args();root=Path(__file__).resolve().parents[1];frozen=inputs(root)
    output=args.output.resolve();output.mkdir(parents=True,exist_ok=False);results={}
    for workflow in ('exploration','inspector'):
        project=output/workflow/'project'
        if workflow=='exploration':create(project)
        else:shutil.copytree(root/'assets_public/scene_inspector',project,ignore=shutil.ignore_patterns('.azure'))
        results[workflow]={}
        for enabled in (False,True):
            key='enabled' if enabled else 'disabled'
            results[workflow][key]=[run(args.executable.resolve(),project/'project.azureproject',output/workflow/f'{key}-{index+1}',
                enabled,args.samples,args.warmup,not args.skip_cooling) for index in range(args.rounds)]
    assert inputs(root)==frozen,'Source changed during preview comparison'
    result=dict(schemaVersion=1,status='passed' if all(row['passed'] for workflow in results.values() for rows in workflow.values() for row in rows) else 'failed',
                sourceHash=hashlib.sha256(json.dumps(frozen,sort_keys=True).encode()).hexdigest(),workflows=results,
                formalBudget=dict(gpuP95Ms=16.6,editorCpuP95Ms=10,workingSetBytes=2*1024**3,deviceLocalBytes=1.5*1024**3))
    (output/'summary.json').write_text(json.dumps(result,indent=2)+'\n',encoding='utf-8');assert result['status']=='passed'


if __name__=='__main__':main()
