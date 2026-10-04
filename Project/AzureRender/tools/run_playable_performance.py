"""Measure frozen playable loads with warmup, thermal control and budgets."""
import argparse
import csv
import ctypes
import hashlib
import json
import os
import platform
from pathlib import Path
import statistics
import subprocess
import time
from create_playable_project import create
from test_independent_animation_gpu import component

ROOT = Path(__file__).resolve().parents[1]


def stats(values):
    if not values:
        raise ValueError('Missing measurement samples')
    ordered = sorted(values)
    def percentile(p):
        position = (len(ordered)-1)*p
        lo = int(position)
        hi = min(lo+1, len(ordered)-1)
        return ordered[lo]+(ordered[hi]-ordered[lo])*(position-lo)
    return dict(mean=statistics.mean(values), p95=percentile(.95), p99=percentile(.99))


def fixture(root, stress=False, character=None):
    create(root, character)
    path = root/'assets/exploration.azurelevel'
    scene = json.loads(path.read_text(encoding='utf-8'))
    count, lights = (500, 16) if stress else (100, 8)
    if stress:
        for i in range(12):
            scene['nodes'].append(dict(id=f'actor-{i}', resourceId='guide', components={
                'azure.transform': component('azure.transform', {'translation':[(i%4-1.5)*2, 0, -4-i//4*2]}),
                'azure.animator': component('azure.animator', {'asset':'assets:/locomotion.json', 'startTime':i*.13}, 2)}))
    # Expanded exploration contains 21 entities, including 7 Prefab nodes.
    extra = count-21-(12 if stress else 0)
    for i in range(extra):
        scene['nodes'].append(dict(id=f'load-{i}',resourceId='cube',components={
            'azure.transform':component('azure.transform', {'translation':[(i%20-9.5)*.7,.3,-3-i//20*.7], 'scale':[.2,.2,.2]})}))
    scene['lights']=[dict(id=f'point-{i}',nodeId=f'load-{i*3}',color=[1,.8,.65],intensity=1.5,radius=8) for i in range(lights)]
    for node in scene['nodes']:
        if node['id'].startswith('companion-'):
            node['components']['azure.transform']['data']['translation']=[-2 if node['id'].endswith('0') else 2,0,-3]
    if character:
        hero = root/'assets/prefabs/hero.azureprefab'
        data=json.loads(hero.read_text(encoding='utf-8'))
        data['nodes'][0]['components']['azure.character']['data']['centerOffset']=1.0
        data['nodes'][0]['components']['azure.transform']['data']['translation']=[0,0,0]
        hero.write_text(json.dumps(data,indent=2),encoding='utf-8')
    path.write_text(json.dumps(scene,indent=2),encoding='utf-8')
    return dict(entities=count, animatedCharacters=16 if stress else 4, pointLights=lights,
                shadowCascades=4, width=1920,height=1080, fullGameUi=True)


def thermal():
    result=subprocess.run(['nvidia-smi','--query-gpu=temperature.gpu,driver_version,name','--format=csv,noheader,nounits'],capture_output=True,text=True,check=True)
    fields=result.stdout.strip().split(',')
    return dict(temperature=int(fields[0]),driver=fields[1].strip(),gpu=fields[2].strip())


def memory(pid):
    class Counters(ctypes.Structure):
        _fields_=[('cb',ctypes.c_ulong),('faults',ctypes.c_ulong)]+[(x,ctypes.c_size_t) for x in ('peak','working','quotaPeakPaged','quotaPaged','quotaPeakNonPaged','quotaNonPaged','pagefile','peakPagefile')]
    kernel=ctypes.WinDLL('kernel32',use_last_error=True)
    kernel.OpenProcess.restype=ctypes.c_void_p
    kernel.CloseHandle.argtypes=[ctypes.c_void_p]
    psapi=ctypes.WinDLL('psapi');psapi.GetProcessMemoryInfo.argtypes=[ctypes.c_void_p,ctypes.c_void_p,ctypes.c_ulong]
    handle=kernel.OpenProcess(0x410,False,pid)
    if not handle:return 0
    data=Counters();data.cb=ctypes.sizeof(data)
    success=psapi.GetProcessMemoryInfo(handle,ctypes.byref(data),data.cb)
    kernel.CloseHandle(handle)
    return data.peak if success else 0


def run(executable,project,output,editor=False,stress=False,warmup=300,samples=1800):
    initial=thermal()
    while initial['temperature']>55:
        print(f"Cooling GPU: {initial['temperature']} C",flush=True)
        time.sleep(10);initial=thermal()
    timing=output.with_suffix('.json')
    if timing.exists():raise FileExistsError(timing)
    events=[]
    for start in range(320,warmup+samples-180,240):
        events += [{'frame':start,'action':'key','key':68,'down':True},{'frame':start+60,'action':'key','key':68,'down':False},
                   {'frame':start+60,'action':'key','key':65,'down':True},{'frame':start+120,'action':'key','key':65,'down':False}]
    actions=output.with_suffix('.input.json');actions.write_text(json.dumps({'schemaVersion':1,'actions':events}),encoding='utf-8')
    command=[str(executable.resolve()),'--editor-project' if editor else '--project',str(project.resolve()),
             '--width','1920','--height','1080','--fixed-frame-step','--smoke-frames',str(warmup+samples),
             '--game-actions',str(actions.resolve()),'--gpu-timing','--gpu-timing-output',str(timing.resolve())]
    if editor:
        task=output.with_suffix('.editor.json');task.write_text('[{"frame":1,"command":"play"}]',encoding='utf-8')
        command += ['--editor-actions',str(task.resolve())]
    info=subprocess.STARTUPINFO();info.dwFlags|=subprocess.STARTF_USESHOWWINDOW;info.wShowWindow=0
    peak=0;start=time.monotonic()
    with output.with_suffix('.stdout.log').open('w',encoding='utf-8') as stdout,output.with_suffix('.stderr.log').open('w',encoding='utf-8') as stderr:
        process=subprocess.Popen(command,cwd=ROOT,stdout=stdout,stderr=stderr,startupinfo=info)
        while process.poll() is None:
            peak=max(peak,memory(process.pid))
            if time.monotonic()-start>600:
                process.kill();process.wait();raise TimeoutError(command)
            time.sleep(.05)
    if process.returncode:raise RuntimeError(f'Player failed: {process.returncode}, {output}')
    data=json.loads(timing.read_text(encoding='utf-8'))
    assert data['sceneTriangles']>0 and data['skinnedVertices']>0,'Scene complexity must survive renderer unload'
    with timing.with_suffix('.csv').open(encoding='utf-8') as stream:
        gpu=[float(row['frameMs']) for row in csv.DictReader(stream)][warmup:warmup+samples]
    if len(gpu)!=samples:raise ValueError('Incomplete GPU measurement')
    cpu=data['cpuFrame']
    physics=cpu['physicsSamplesMs'][sum(cpu['physicsStepCounts'][:warmup]):]
    result={'gpu':stats(gpu),'cpu':stats(cpu['workSamplesMs'][warmup:]),'wait':stats(cpu['waitSamplesMs'][warmup:]),
            'physics':stats(physics),'initialDevice':initial,'peakWorkingSetBytes':peak,
            'allocator':data['allocator'],'submission':data['submission'], 'samples':samples,'warmup':warmup,
            'complexity':{key:data[key] for key in ('uniqueMeshes','sceneTriangles','skinnedVertices','transparentTriangles','jointCapacity','vertexCapacity')},
            'sceneExtent':[data['sceneWidth'],data['sceneHeight']]}
    gpu_limit,cpu_limit=(25,12) if stress else (16.6,10 if editor else 8)
    result['passed']=result['gpu']['p95']<=gpu_limit and result['cpu']['p95']<=cpu_limit and result['physics']['p95']<=2
    if not stress and 'private' not in output.name:
        result['passed'] &= peak<=2*1024**3 and data['allocator']['deviceLocalPeakBytes']<=1.5*1024**3
    print(output.name,json.dumps({k:result[k] for k in ('gpu','cpu','physics','passed')}),flush=True)
    return result


def main():
    parser=argparse.ArgumentParser(__doc__)
    parser.add_argument('--player',type=Path,required=True);parser.add_argument('--editor',type=Path,required=True)
    parser.add_argument('--character',type=Path);parser.add_argument('--output',type=Path,required=True)
    args=parser.parse_args();args.output.mkdir(parents=True,exist_ok=True)
    from build_provenance import inputs
    frozen_inputs=inputs(ROOT)
    source_hash=hashlib.sha256(json.dumps(frozen_inputs,sort_keys=True).encode()).hexdigest()
    loads=[('standard',False,None,False),('editor',False,None,True),('stress',True,None,False)]
    if args.character:loads.append(('private',False,args.character,False))
    results={};fingerprints={};quality={}
    for name,stress,character,editor in loads:
        folder=args.output/name;quality[name]=fixture(folder,stress,character)
        fingerprints[name]={str(p.relative_to(folder)):hashlib.sha256(p.read_bytes()).hexdigest() for p in sorted((folder/'assets').rglob('*')) if p.is_file()}
        results[name]=[run(args.editor if editor else args.player,folder/'project.azureproject',args.output/f'{name}-{i+1}',editor,stress) for i in range(3)]
    source=subprocess.run(['git','rev-parse','HEAD'],cwd=ROOT,capture_output=True,text=True,check=True).stdout.strip()
    if inputs(ROOT)!=frozen_inputs:raise RuntimeError('Source changed during performance measurement')
    summary=dict(schemaVersion=1,sourceCommit=source,sourceHash=source_hash,host=platform.platform(),quality=quality,sceneHashes=fingerprints,rounds=results,
                 median={name:{key:statistics.median(r[key]['p95'] for r in rounds) for key in ('gpu','cpu','physics')} for name,rounds in results.items()},
                 passed=all(r['passed'] for rounds in results.values() for r in rounds))
    (args.output/'summary.json').write_text(json.dumps(summary,indent=2),encoding='utf-8')
    return 0 if summary['passed'] else 1


if __name__=='__main__':raise SystemExit(main())
