"""Diagnostic only: capture large virtual allocation callers in an owned process."""
import hashlib, json, os, subprocess, time, sys, ctypes
from ctypes import wintypes
from pathlib import Path
ROOT = Path.cwd().resolve()
WORK = ROOT / 'build/implementation-2026-10-10'
sys.path.insert(0,str(WORK/'diagnostic-deps'))
import psutil
OUT = WORK / 'p5-allocation-debug-residency'
OUT.mkdir(exist_ok=False)
exe = ROOT / 'build/ninja-msvc-release/AzureRender.exe'
debugger = Path('C:/Program Files/LLVM/bin/lldb.exe')
def digest(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()
before = digest(exe)
actions = json.loads((WORK/'p5-performance/editor-1.input.json').read_text())
(OUT/'input.json').write_text(json.dumps(actions), encoding='utf-8')
(OUT/'editor.json').write_text('[{"frame":1,"command":"play"}]', encoding='utf-8')
quote = lambda p: '"' + str(p).replace('\\', '/') + '"'
commands = [
    'settings set auto-confirm true',
    'target create ' + quote(exe),
    'process launch -s -w ' + quote(ROOT) + ' -o ' + quote(OUT/'player.stdout.log') +
        ' -e ' + quote(OUT/'player.stderr.log') + ' -- ' + ' '.join([
            '--editor-close-policy save', '--editor-project', quote(WORK/'p5-performance/editor/project.azureproject'),
            '--width 1920 --height 1080 --fixed-frame-step --smoke-frames 2100',
            '--game-actions', quote(OUT/'input.json'), '--editor-actions', quote(OUT/'editor.json'),
            '--gpu-timing --gpu-timing-output', quote(OUT/'timing.json')]),
    'breakpoint set -n NtAllocateVirtualMemory -s ntdll.dll',
    'breakpoint command add 1',
    'register read rcx rdx r8 r9 rsp',
    'memory read -s 8 -f x -c 1 $r9',
    'memory read -s 8 -f x -c 1 $rdx',
    'memory read -s 8 -f x -c 2 `$rsp+0x28`',
    'thread backtrace -c 18',
    'continue',
    'DONE',
    'continue',
    'breakpoint list', 'process status', 'image list', 'quit'
]
source = OUT/'commands.lldb'
source.write_text('\n'.join(commands)+'\n', encoding='utf-8')
env = dict(os.environ, AZURERENDER_EDITOR_CONFIG=str(OUT/'config'))
start = time.monotonic()
observations=[]
snapshots=[]
class Region(ctypes.Structure):
    _fields_=[('base',ctypes.c_void_p),('allocation',ctypes.c_void_p),('allocationProtect',ctypes.c_ulong),('size',ctypes.c_size_t),('state',ctypes.c_ulong),('protect',ctypes.c_ulong),('kind',ctypes.c_ulong)]
kernel=ctypes.WinDLL('kernel32',use_last_error=True)
kernel.OpenProcess.restype=ctypes.c_void_p
kernel.CloseHandle.argtypes=[ctypes.c_void_p]
kernel.VirtualQueryEx.argtypes=[ctypes.c_void_p,ctypes.c_void_p,ctypes.c_void_p,ctypes.c_size_t]
psapi=ctypes.WinDLL('psapi',use_last_error=True)
psapi.QueryWorkingSet.argtypes=[ctypes.c_void_p,ctypes.c_void_p,ctypes.c_ulong]
psapi.GetMappedFileNameW.argtypes=[ctypes.c_void_p,ctypes.c_void_p,wintypes.LPWSTR,ctypes.c_ulong]
def snapshot(pid,observation):
    handle=kernel.OpenProcess(0x410,False,pid)
    if not handle:return
    try:
        buffer=(ctypes.c_size_t*(1024*1024))()
        if not psapi.QueryWorkingSet(handle,buffer,ctypes.sizeof(buffer)):return
        pages=sorted((int(buffer[i])>>12)<<12 for i in range(1,int(buffer[0])+1))
        regions={};upper=0;row=None
        for address in pages:
            if address>=upper:
                region=Region()
                if not kernel.VirtualQueryEx(handle,address,ctypes.byref(region),ctypes.sizeof(region)):continue
                upper=region.base+region.size
                key=(region.allocation,region.kind)
                if key not in regions:
                    path=ctypes.create_unicode_buffer(1024)
                    psapi.GetMappedFileNameW(handle,address,path,len(path))
                    regions[key]=dict(allocation=region.allocation,kind=region.kind,path=path.value,residentBytes=0,regionBytes=0)
                row=regions[key];row['regionBytes']+=region.size
            if row:row['residentBytes']+=4096
        snapshots.append(dict(**observation,regions=list(regions.values())))
    finally:kernel.CloseHandle(handle)
last_snapshot=0
with (OUT/'debugger.log').open('w',encoding='utf-8') as stream:
    process = subprocess.Popen([str(debugger), '-b', '-s', str(source)], cwd=ROOT,
                               env=env, stdout=stream, stderr=subprocess.STDOUT)
    while process.poll() is None:
        try:
            for child in psutil.Process(process.pid).children(recursive=True):
                if child.name().lower()!='azurerender.exe':continue
                memory=child.memory_info()
                observation=dict(seconds=time.monotonic()-start,pid=child.pid,
                    workingSet=memory.rss,privateBytes=memory.private,
                    logBytes=(OUT/'debugger.log').stat().st_size)
                observations.append(observation)
                if observation['seconds']-last_snapshot>=1:
                    snapshot(child.pid,observation);last_snapshot=observation['seconds']
        except (psutil.NoSuchProcess,psutil.AccessDenied):pass
        if time.monotonic()-start>600:
            for child in psutil.Process(process.pid).children(recursive=True):child.kill()
            process.kill()
            process.wait()
            break
        time.sleep(.1)
assert before == digest(exe), 'Tested executable changed'
summary = dict(diagnosticOnly=True, elapsedSeconds=time.monotonic()-start,
               debugger=str(debugger), debuggerSha256=digest(debugger),
               executableSha256=before, debuggerExitCode=process.returncode,
               completed=(OUT/'timing.json').is_file(), unchangedBudgets=True,
               covers='NtAllocateVirtualMemory requests; not all heap allocations')
(OUT/'memory.json').write_text(json.dumps(observations,indent=2)+'\n',encoding='utf-8')
(OUT/'pages.json').write_text(json.dumps(snapshots,indent=2)+'\n',encoding='utf-8')
(OUT/'summary.json').write_text(json.dumps(summary,indent=2)+'\n',encoding='utf-8')
print(json.dumps(summary), flush=True)
