import gzip,hashlib,json,re
from pathlib import Path
root=Path.cwd();run=Path(__file__).resolve().parent;script=root/'tools/test_material_lighting_gpu.py';(run/'red-public-script.py.gz').write_bytes(gzip.compress(script.read_bytes(),mtime=0));log=(run/'red-public-gpu/indexed-generic-diffuse-global/player.log').read_text();assert 'unknown field environmentLighting' in log;report=json.loads((run/'red-public-gpu/summary.json').read_text());assert report['status']=='failed' and report['cases'][0]['exitCode']!=0;(run/'red-public.json').write_bytes((json.dumps(dict(status='red-confirmed',programSha256=report['playerSha256'],scriptSha256=report['scriptSha256'],reason='Production Vulkan Player rejects the missing material environment source contract.'),indent=2)+'\n').encode());changes=[]
for p in sorted((root/'tools').glob('test_*gpu.py')):
 old=p.read_bytes();text=old.decode();updated,count=re.subn(r"(materialSurfaceProfileLayoutVersion['\"]\]\s*==\s*)19\b",r'\g<1>20',text)
 if not count:continue
 q=run/'before'/(p.name+'.gz');q.parent.mkdir(exist_ok=True);q.write_bytes(gzip.compress(old,mtime=0));data=updated.encode()
 with p.open('r+b') as f:f.write(data);f.truncate()
 changes.append(dict(path=p.relative_to(root).as_posix(),assertions=count,beforeSha256=hashlib.sha256(old).hexdigest(),afterSha256=hashlib.sha256(data).hexdigest()))
(run/'layout-tool-updates.json').write_bytes((json.dumps(changes,indent=2)+'\n').encode());print('Updated layout assertions in',len(changes),'tools; thresholds retained')
