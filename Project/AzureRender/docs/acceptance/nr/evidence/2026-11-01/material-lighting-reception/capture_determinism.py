import argparse,hashlib,json,os,re,subprocess,time
from pathlib import Path
parser=argparse.ArgumentParser();parser.add_argument('--name',choices=['serial-a','serial-b'],required=True);args=parser.parse_args();root=Path.cwd();run=Path(__file__).resolve().parent;out=run/'determinism-captures'/args.name;out.mkdir(parents=True);player=root/'build/ninja-msvc-release/AzurePlayer.exe';build=json.loads((run/'release-build.json').read_text());sha=lambda p:hashlib.sha256(p.read_bytes()).hexdigest();assert sha(player)==build['programs']['AzurePlayer'];assert {p.name:sha(p) for p in (player.parent/'shaders').glob('*.spv')}==build['shaders']
command=[str(player),'--project',str(run/'candidate-project/full-world-hidden.azureproject'),'--width','2560','--height','1440','--capture-frames','12','--capture-fps','60','--fixed-frame-step','--qa-camera','scene','--qa-light','scene','--capture-dir',str(out/'frames'),'--runtime-report',str(out/'runtime.json'),'--disable-parallel-recording'];env=os.environ.copy();env['DOTNET_ROOT']='D:/Tools/dotnet9';start=time.perf_counter();record=dict(status='running',command=command,programSha256=sha(player),shaders=build['shaders'])
try:
    with (out/'player.log').open('wb') as log:r=subprocess.run(command,cwd=out,stdout=log,stderr=log,env=env,timeout=700)
    assert r.returncode==0;assert not re.search(r'VUID-|Validation (?:Error|Warning)|SYNC-HAZARD|\[error\]',(out/'player.log').read_text(errors='replace'),re.I)
    logs=[json.loads(s) for s in (out/'captures/azureplayer.log.jsonl').read_text().splitlines()];assert not any(row['level']=='error' or row['subsystem']=='validation' and row['level']=='warning' for row in logs);assert any(row['message']=='Allocator after unload: buffers=0 images=0' for row in logs)
    current={p.name:sha(p) for p in (out/'frames').glob('frame_*.png')};assert len(current)==12
    prior=root/'build/runs/20261101-043757-material-display-transform-93d6e0/native-final-captures/full-world-hidden/frames';first=run/'native-captures/full-world-hidden/frames';record.update(status='captured',seconds=time.perf_counter()-start,images=current,versusFirstDifferentFrames=[name for name,d in current.items() if sha(first/name)!=d],versusFrozenDifferentFrames=[name for name,d in current.items() if sha(prior/name)!=d])
finally:
    if record['status']=='running':record['status']='failed'
    (run/('determinism-'+args.name+'.json')).write_bytes((json.dumps(record,indent=2)+'\n').encode());print(json.dumps({k:v for k,v in record.items() if k not in {'shaders','command','images'}}))
