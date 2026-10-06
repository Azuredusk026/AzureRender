"""Run a moved game for real wall time and close its own window normally."""
import argparse
import ctypes
from ctypes import wintypes
import hashlib
import json
import os
from pathlib import Path
import statistics
import subprocess
import time
from build_game import verify_package, digest
from run_playable_performance import stats


def periodic_statistics(rows,warm_frame):
    periodic=[row for row in rows if row['frame']>warm_frame+240 and row['frame']%240==0]
    commits=[row for row in rows if row['committed'] and row['frame']>1]
    return dict(periodicSampleCount=len(periodic),
        periodicCpu=stats([row['workMs'] for row in periodic]),
        periodicPhysics=stats([row['physicsMaxMs'] for row in periodic]),
        commitExtra=stats([row['commitMs'] for row in commits]),
        commitFrame=stats([row['workMs']+row['waitMs'] for row in commits]))


def evaluate(data, formal=True):
    assert data['realTime'] and data['actualSeconds'] >= (1800 if formal else 0), 'Real 30-minute run required'
    assert data['frames'] > 0 and data['exitCode'] == 0 and data['released'], 'Normal exit and release required'
    assert not data['validationErrors'] and not data['scriptErrors'] and not data['presentationErrors'] and not data['loadErrors'], 'Runtime errors'
    windows=data['windowChecks']
    assert all(windows[key] for key in ('minimized','restored','resized')), 'Window recovery incomplete'
    assert windows['finalExtent']==[1920,1080], 'Final viewport must be 1080p'
    if formal:assert data['switches'] >= 10 and data['restarts'] >= 10, 'At least ten switches and restarts required'
    assert data['gpu']['p95'] <= 16.6 and data['cpu']['p95'] <= 8 and data['physics']['p95'] <= 2, 'Standard budget exceeded'
    assert data['periodicCpu']['p95'] <= 8 and data['periodicPhysics']['p95'] <= 2, 'Whole-run CPU budget exceeded'
    assert data['commitExtra']['p99'] <= 8 and data['commitFrame']['p99'] <= 33.3, 'Preloaded commit budget exceeded'
    assert data['gpuWindows'] and all(window['p95']<=16.6 for window in data['gpuWindows']), 'Whole-run GPU budget exceeded'
    assert all(value <= .05 for value in data['residencyGrowth'].values()), 'Resident resources grew more than 5%'
    assert data['workingSetGrowth'] <= .05, 'Working set grew more than 5%'
    assert 0 < data['peakWorkingSetBytes'] <= 2*1024**3 and data['deviceLocalPeakBytes'] <= 1.5*1024**3, 'Memory capacity budget exceeded'


def isolated_environment():
    env={key.upper():value for key,value in os.environ.items() if not key.upper().startswith(('AZURERENDER_','AZURE_CHARACTER_'))}
    env['PATH']=str(Path(env['SYSTEMROOT'])/'System32')
    return env


def inputs(path):
    actions=[]
    def key(frame,value,down):actions.append(dict(frame=frame,action='key',key=value,down=down))
    def press(frame,value):key(frame,value,True);key(frame+1,value,False)
    for index in range(500):
        base=20+index*6000
        press(base,82);press(base+30,69)
        key(base+60,87,True);key(base+2400,87,False);press(base+2405,69)
        press(base+2500,82)
        actions.append(dict(frame=base+2700,action='preload-level',reference='assets:/alternate.azurelevel'))
        actions.append(dict(frame=base+3000,action='load-level',reference='assets:/alternate.azurelevel'))
        press(base+3030,69);key(base+3060,87,True);key(base+5400,87,False)
        actions.append(dict(frame=base+5700,action='preload-level',reference='assets:/exploration.azurelevel'))
        actions.append(dict(frame=base+5900,action='load-level',reference='assets:/exploration.azurelevel'))
    actions.sort(key=lambda event:event['frame'])
    path.write_text(json.dumps(dict(schemaVersion=1,actions=actions)),encoding='utf-8')
    return actions


class NativeWindow:
    def __init__(self,pid,title_prefix='AzureRender'):
        self.pid=pid;self.user=ctypes.WinDLL('user32',use_last_error=True)
        self.title_prefix=title_prefix
        self.user.GetWindowTextW.argtypes=[wintypes.HWND,wintypes.LPWSTR,ctypes.c_int]
        self.user.GetWindowThreadProcessId.argtypes=[wintypes.HWND,ctypes.POINTER(wintypes.DWORD)]
        self.user.GetClientRect.argtypes=[wintypes.HWND,ctypes.POINTER(wintypes.RECT)]
        self.user.GetWindowRect.argtypes=[wintypes.HWND,ctypes.POINTER(wintypes.RECT)]
        self.user.ShowWindow.argtypes=[wintypes.HWND,ctypes.c_int]
        self.user.IsIconic.argtypes=[wintypes.HWND]
        self.user.SetWindowPos.argtypes=[wintypes.HWND,wintypes.HWND,ctypes.c_int,ctypes.c_int,ctypes.c_int,ctypes.c_int,wintypes.UINT]
        self.user.PostMessageW.argtypes=[wintypes.HWND,wintypes.UINT,wintypes.WPARAM,wintypes.LPARAM]
        self.callback=ctypes.WINFUNCTYPE(wintypes.BOOL,wintypes.HWND,wintypes.LPARAM)
        self.user.EnumWindows.argtypes=[self.callback,wintypes.LPARAM]
        self.handle=None

    def find(self):
        self.handle=None
        def visit(handle,_):
            process=wintypes.DWORD();self.user.GetWindowThreadProcessId(handle,ctypes.byref(process))
            rect=wintypes.RECT()
            title=ctypes.create_unicode_buffer(512);self.user.GetWindowTextW(handle,title,512)
            if process.value==self.pid and title.value.startswith(self.title_prefix) and self.user.GetClientRect(handle,ctypes.byref(rect)) and rect.right>200 and rect.bottom>100:
                self.handle=handle;return False
            return True
        self.user.EnumWindows(self.callback(visit),0)
        return self.handle

    def extent(self):
        rect=wintypes.RECT();assert self.user.GetClientRect(self.handle,ctypes.byref(rect))
        return [rect.right-rect.left,rect.bottom-rect.top]

    def hide(self):self.user.ShowWindow(self.handle,0)

    def minimize(self):
        self.user.ShowWindow(self.handle,6)
        return bool(self.user.IsIconic(self.handle))

    def restore(self):
        self.user.ShowWindow(self.handle,4)
        if self.user.IsIconic(self.handle):self.user.ShowWindow(self.handle,9)
        restored=not self.user.IsIconic(self.handle);self.hide()
        return restored

    def resize(self,width,height):
        outer=wintypes.RECT();assert self.user.GetWindowRect(self.handle,ctypes.byref(outer))
        client=self.extent()
        assert self.user.SetWindowPos(self.handle,None,0,0,width+outer.right-outer.left-client[0],height+outer.bottom-outer.top-client[1],0x16)
        return self.extent()==[width,height]

    def close(self):assert self.user.PostMessageW(self.handle,0x10,0,0)


def process_memory(pid):
    class Counters(ctypes.Structure):
        _fields_=[('cb',ctypes.c_ulong),('faults',ctypes.c_ulong)]+[(key,ctypes.c_size_t) for key in ('peak','working','quotaPeakPaged','quotaPaged','quotaPeakNonPaged','quotaNonPaged','pagefile','peakPagefile')]
    kernel=ctypes.WinDLL('kernel32',use_last_error=True);kernel.OpenProcess.restype=ctypes.c_void_p
    kernel.CloseHandle.argtypes=[ctypes.c_void_p]
    psapi=ctypes.WinDLL('psapi');psapi.GetProcessMemoryInfo.argtypes=[ctypes.c_void_p,ctypes.c_void_p,ctypes.c_ulong]
    handle=kernel.OpenProcess(0x410,False,pid)
    if not handle:raise ctypes.WinError(ctypes.get_last_error())
    try:
        data=Counters();data.cb=ctypes.sizeof(data)
        assert psapi.GetProcessMemoryInfo(handle,ctypes.byref(data),data.cb)
        return dict(working=data.working,peak=data.peak)
    finally:kernel.CloseHandle(handle)


def run(package,output,seconds=1800,probe=False):
    package=package.resolve();output=output.resolve();output.mkdir(parents=True,exist_ok=True)
    assert os.name=='nt' and seconds >= (15 if probe else 1800)
    verify_package(package)
    assert (package/'game/assets/alternate.azurelevel').is_file(), 'Alternating exploration level required'
    quality=json.loads((package/'QUALITY.json').read_text(encoding='utf-8'))
    assert quality['entities']==100 and quality['animatedCharacters']==4 and quality['pointLights']==8
    from build_provenance import inputs as source_inputs
    source=Path(__file__).resolve().parents[1];frozen=source_inputs(source)
    actions=inputs(output/'input.json');timing=output/'timing.json';runtime=output/'runtime.json'
    assert not timing.exists(), 'Use a fresh evidence directory'
    env=isolated_environment()
    command=[str(package/'bin/AzurePlayer.exe'),'--project',str(package/'game/project.azureproject'),
        '--resource-root',str(package/'share/AzureRender'),'--width','1920','--height','1080',
        '--game-actions',str(output/'input.json'),'--gpu-timing','--gpu-timing-output',str(timing),'--runtime-report',str(runtime)]
    csv_path=timing.with_suffix('.csv');observations=[];gpu=[];gpu_windows=[];minute=[];pending=b'';csv_offset=0
    checks=dict(minimized=False,restored=False,resized=False,finalExtent=[]);peak=0;warm_frame=0;window_events=[]
    warm_seconds=max(11,min(120,seconds/3))
    info=subprocess.STARTUPINFO();info.dwFlags|=subprocess.STARTF_USESHOWWINDOW;info.wShowWindow=0
    try:ctypes.WinDLL('user32').SetProcessDPIAware()
    except AttributeError:pass
    with (output/'stdout.log').open('w',encoding='utf-8') as stdout,(output/'stderr.log').open('w',encoding='utf-8') as stderr:
        process=subprocess.Popen(command,cwd=package,env=env,stdout=stdout,stderr=stderr,startupinfo=info)
        window=NativeWindow(process.pid);startup=time.monotonic()
        try:
            while not window.find():
                assert process.poll() is None and time.monotonic()-startup<60, 'Player window initialization failed'
                time.sleep(.1)
            window.hide();start=time.monotonic();next_minute=60;small=False;large=False
            while True:
                elapsed=time.monotonic()-start
                assert process.poll() is None, 'Player exited before the real-time duration'
                if elapsed>=3 and not checks['minimized']:
                    checks['minimized']=window.minimize();window_events.append(dict(seconds=elapsed,event='minimize',passed=checks['minimized']))
                if elapsed>=5 and not checks['restored']:
                    checks['restored']=window.restore();window_events.append(dict(seconds=elapsed,event='restore',passed=checks['restored']))
                if elapsed>=7 and not small:
                    small=window.resize(960,540);window_events.append(dict(seconds=elapsed,event='resize',extent=window.extent(),passed=small))
                if elapsed>=9 and not large:
                    large=window.resize(1920,1080);checks['resized']=small and large
                    window_events.append(dict(seconds=elapsed,event='resize',extent=window.extent(),passed=large))
                mem=process_memory(process.pid);peak=max(peak,mem['peak'])
                if csv_path.exists():
                    with csv_path.open('rb') as stream:
                        stream.seek(csv_offset);pending+=stream.read();csv_offset=stream.tell()
                    lines=pending.split(b'\n');pending=lines.pop()
                    for line in lines:
                        if line and not line.startswith(b'frame,'):
                            value=float(line.split(b',')[-1]);gpu.append(value);minute.append(value)
                observations.append(dict(seconds=elapsed,gpuFrames=len(gpu),**mem))
                if elapsed<warm_seconds:warm_frame=len(gpu)
                if elapsed>=next_minute:
                    gpu_windows.append(dict(endSeconds=elapsed,frames=len(minute),**stats(minute)));minute=[];next_minute+=60
                    print(json.dumps(dict(seconds=round(elapsed),gpuFrames=len(gpu),workingMiB=round(mem['working']/1024**2,1))),flush=True)
                    (output/'progress.json').write_text(json.dumps(observations[-1]),encoding='utf-8')
                if elapsed>=seconds:break
                time.sleep(.5)
            checks['finalExtent']=window.extent();window.close();actual=time.monotonic()-start
            assert process.wait(timeout=60)==0, 'Normal WM_CLOSE shutdown failed'
        finally:
            if process.poll() is None:
                if window.handle:
                    window.restore();window.close()
                    try:process.wait(timeout=20)
                    except subprocess.TimeoutExpired:process.kill();process.wait()
                else:process.kill();process.wait()
    # The final flushed CSV is authoritative, including shutdown frames.
    import csv
    with csv_path.open(encoding='utf-8') as stream:all_gpu=[float(row['frameMs']) for row in csv.DictReader(stream)]
    if minute:gpu_windows.append(dict(endSeconds=actual,frames=len(minute),**stats(minute)))
    data=json.loads(timing.read_text(encoding='utf-8'));game=json.loads(runtime.read_text(encoding='utf-8'))
    resources=data['resourceFrames'];stable=[row for row in resources if row['frame']>warm_frame+240 and not row['committed'] and not row['loading']]
    assert stable, 'No stable residency samples'
    growth={field:max(row[field] for row in stable)/min(row[field] for row in stable)-1 for field in ('buffers','images','bufferBytes','imageBytes')}
    memory_stable=[row for row in observations if row['seconds']>=warm_seconds]
    count=max(1,min(120,len(memory_stable)//4))
    first=statistics.median(row['working'] for row in memory_stable[:count]);last=statistics.median(row['working'] for row in memory_stable[-count:])
    consumed=actions[:game['replayedActions']];switches=sum(row['action']=='load-level' for row in consumed)
    restarts=sum(row['action']=='key' and row['key']==82 and row['down'] for row in consumed)
    commits=[row for row in resources if row['committed'] and row['frame']>1]
    assert len(commits)==switches+restarts, (len(commits),switches,restarts)
    log=(output/'stdout.log').read_text(encoding='utf-8')+'\n'+(output/'stderr.log').read_text(encoding='utf-8')
    cpu=data['cpuFrame']
    summary=dict(status='measured',actualSeconds=actual,realTime='--fixed-frame-step' not in command,
        exitCode=process.returncode,frames=data['samples'],switches=switches,restarts=restarts,commits=len(commits),
        validationErrors=log.count('Validation Error')+log.count('VUID-'),released='Allocator after unload: buffers=0 images=0' in log,
        scriptErrors=game['scriptErrors'],presentationErrors=game['presentationErrors'],loadErrors=sorted(set(row['loadError'] for row in resources if row['loadError'])),
        windowChecks=checks,windowEvents=window_events,gpu=stats(all_gpu[300:]),cpu=stats(cpu['workSamplesMs']),physics=stats(cpu['physicsSamplesMs']),
        gpuWindows=gpu_windows,residencyGrowth=growth,firstResidency=stable[0],lastResidency=stable[-1],
        workingSetGrowth=max(0,last/first-1),workingSetBaselineBytes=first,workingSetFinalBytes=last,
        peakWorkingSetBytes=peak,deviceLocalPeakBytes=data['allocator']['deviceLocalPeakBytes'],
        isolatedPath=env['PATH'],packageManifestHash=digest(package/'game_manifest.json'),
        sourceHash=hashlib.sha256(json.dumps(frozen,sort_keys=True).encode()).hexdigest(),quality=quality,
        cpuRetainedFrames=len(cpu['workSamplesMs']),cpuSampleOffset=cpu['sampleOffsetFrame'],warmupSeconds=warm_seconds,warmupFrame=warm_frame,
        runtimeHash=digest(package/'bin/AzurePlayer.exe'))
    summary.update(periodic_statistics(resources,warm_frame))
    assert data['sceneWidth']==1920 and data['sceneHeight']==1080
    assert data['samples']==len(all_gpu)
    assert source_inputs(source)==frozen, 'Source changed during long run'
    verify_package(package)
    (output/'observation.json').write_text(json.dumps(observations,indent=2),encoding='utf-8')
    try:evaluate(summary,formal=not probe)
    except AssertionError:
        summary['status']='failed'
        (output/'summary.json').write_text(json.dumps(summary,indent=2)+'\n',encoding='utf-8')
        raise
    summary['status']='probe' if probe else 'passed'
    (output/'summary.json').write_text(json.dumps(summary,indent=2)+'\n',encoding='utf-8')
    print(json.dumps(summary),flush=True)
    return summary


def reanalyse(package,output,provenance):
    source=Path(__file__).resolve().parents[1];package=package.resolve();output=output.resolve()
    original=output/'measurement-original.json'
    if not original.exists():original.write_bytes((output/'summary.json').read_bytes())
    summary=json.loads(original.read_text(encoding='utf-8'))
    verify_package(package)
    assert summary['packageManifestHash']==digest(package/'game_manifest.json'), 'Measured package changed'
    proof=json.loads(provenance.read_text(encoding='utf-8'))
    from build_provenance import inputs as source_inputs
    current=source_inputs(source)
    runtime_inputs=[key for key in proof['source'] if key.startswith(('src/','shaders/','schemas/','assets_public/')) or key=='CMakeLists.txt']
    assert all(current[key]==proof['source'][key] for key in runtime_inputs), 'Measured runtime source changed'
    expected=next(value for name,value in proof['products'].items() if Path(name).name=='AzurePlayer.exe')
    assert digest(package/'bin/AzurePlayer.exe')==expected, 'Measured runtime binary changed'
    timing=output/'timing.json';data=json.loads(timing.read_text(encoding='utf-8'))
    observation=json.loads((output/'observation.json').read_text(encoding='utf-8'))
    warm_frame=summary.get('warmupFrame',max(row['gpuFrames'] for row in observation if row['seconds']<summary['warmupSeconds']))
    summary.update(periodic_statistics(data['resourceFrames'],warm_frame))
    summary['measurementSourceHash']=summary['sourceHash'];summary['analysisSourceHash']=hashlib.sha256(json.dumps(current,sort_keys=True).encode()).hexdigest()
    summary['runtimeHash']=expected;summary['warmupFrame']=warm_frame
    summary['rawEvidenceHashes']={name:digest(output/name) for name in ('measurement-original.json','timing.json','timing.csv','runtime.json','observation.json','stdout.log','stderr.log')}
    evaluate(summary,formal=summary['actualSeconds']>=1800)
    summary['status']='passed' if summary['actualSeconds']>=1800 else 'probe'
    (output/'summary.json').write_text(json.dumps(summary,indent=2)+'\n',encoding='utf-8')
    print(json.dumps(summary),flush=True)
    return summary


if __name__=='__main__':
    parser=argparse.ArgumentParser(__doc__);parser.add_argument('--package',required=True,type=Path)
    parser.add_argument('--output',required=True,type=Path);parser.add_argument('--seconds',type=float,default=1800)
    parser.add_argument('--probe',action='store_true');parser.add_argument('--reanalyse',action='store_true')
    parser.add_argument('--provenance',type=Path);args=parser.parse_args()
    if args.reanalyse:
        if not args.provenance:parser.error('--reanalyse requires --provenance')
        reanalyse(args.package,args.output,args.provenance)
    else:run(args.package,args.output,args.seconds,args.probe)
