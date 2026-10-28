from pathlib import Path
import subprocess,json,hashlib
r=Path(__file__).resolve().parent
folder=r/'fixed-cloth';folder.mkdir(exist_ok=True)
args=json.loads((r/'moving-cloth-debug/candidate/command.json').read_text())
args[args.index('--capture-dir')+1]=str(folder/'frames')
args+=['--disable-bindless','--disable-compute-skinning','--disable-parallel-recording']
(folder/'command.json').write_text(json.dumps(args,indent=2))
with (folder/'player.log').open('w') as log:subprocess.run(args,stdout=log,stderr=subprocess.STDOUT,check=True,timeout=180)
images=sorted((folder/'frames').glob('frame_*.png'));assert len(images)==80
same=sum(p.read_bytes()==(r/'moving-cloth-debug/candidate/frames'/p.name).read_bytes() for p in images)
assert same==80
log=(folder/'player.log').read_text();assert 'Allocator after unload: buffers=0 images=0' in log and 'VUID-' not in log
(r/'fixed-compatibility.json').write_text(json.dumps(dict(frames=80,identicalPairs=same),indent=2))
print('Fixed descriptor 80 frames exact')
