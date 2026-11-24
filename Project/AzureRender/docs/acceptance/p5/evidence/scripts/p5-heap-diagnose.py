"""Trace heap ownership in one affected workload without changing production settings."""
import ctypes, json, os, subprocess, sys, time
from pathlib import Path
ROOT=Path.cwd();WORK=ROOT/'build/implementation-2026-10-10';OUT=WORK/'p5-heap-diagnose'
OUT.mkdir(exist_ok=False);sys.path.insert(0,str(ROOT/'tools'))
import run_playable_performance as measurement
base_memory=measurement.memory;started=False;attempted=False;begin=None

def memory(pid):
    global started, attempted, begin
    if begin is None:begin=time.monotonic()
    if time.monotonic()-begin>=3 and not attempted:
        attempted=True
        args=['xperf','-start','AzureP5Heap','-heap','-pids',str(pid),'-stackwalk','HeapAlloc+HeapRealloc','-f',str(OUT/'heap.etl'),'-MaxFile','256','-FileMode','Circular']
        result=subprocess.run(args,capture_output=True,text=True)
        (OUT/'trace-start.json').write_text(json.dumps(dict(command=args,exitCode=result.returncode,stdout=result.stdout,stderr=result.stderr),indent=2))
        started=result.returncode==0
    return base_memory(pid)
measurement.memory=memory
os.environ['AZURERENDER_EDITOR_CONFIG']=str(OUT/'config')
try:
    result=measurement.run(ROOT/'build/ninja-msvc-release/AzureRender.exe',WORK/'p5-performance/editor/project.azureproject',OUT/'editor',True,False)
    (OUT/'result.json').write_text(json.dumps(result,indent=2))
finally:
    if started:
        result=subprocess.run(['xperf','-stop','AzureP5Heap'],capture_output=True,text=True)
        (OUT/'trace-stop.json').write_text(json.dumps(dict(exitCode=result.returncode,stdout=result.stdout,stderr=result.stderr),indent=2))
print('heap diagnostic ended; trace started',started,flush=True)
