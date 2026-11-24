import hashlib,json
from pathlib import Path
root=Path.cwd();run=Path(__file__).resolve().parent;read=lambda p:json.loads(p.read_text());sha=lambda p:hashlib.sha256(p.read_bytes()).hexdigest()
for cfg in ['release','debug']:
    b=read(run/(cfg+'-build.json'));player=root/('build/ninja-msvc-'+cfg)/'AzurePlayer.exe'
    assert b['status']=='built-contract-tests-passed' and len(b['tests'])==17
    assert sha(player)==b['programs']['AzurePlayer']
    assert {p.name:sha(p) for p in (player.parent/'shaders').glob('*.spv')}==b['shaders']
    for name,digest in b['sourceFiles'].items():assert hashlib.sha256((root/name).read_bytes().replace(b'\r\n',b'\n')).hexdigest()==digest,name
    s=read(run/'public-gpu'/cfg/'summary.json');assert s['status']=='passed' and len(s['cases'])==84 and s['shaders']==b['shaders'] and s['playerSha256']==sha(player)
    assert all(c['status']=='passed' and c['validationEnabled']==(cfg=='debug') for c in s['cases'])
    print(cfg,len(b['sourceFiles']),'compiled inputs,',len(s['cases']),'GPU captures; maximum closed-form error',max(c.get('maximumLinearError',0) for c in s['cases']))
protected=read(root/'build/runs/20261031-053326-startup-context-d7dce6/preserved-user-files.json')
for name,digest in protected.items():assert sha(root/name)==digest,name
print('Protected user files unchanged:',len(protected))
for path in (run/'native-captures').glob('*/frames/capture_manifest.json'):
    d=read(path);print(path.parent.parent.name,'manifest fields',list(d)[:12])
