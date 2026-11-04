from pathlib import Path
import os,json,subprocess,sys,re,hashlib
r=Path(__file__).resolve().parent;root=r.parents[2];p=r/'qualified-lit-surfaces-playable/playable.azureproject';motion=json.loads((r/'lit-surfaces-joint-motion-results.json').read_bytes());assert motion['status']=='passed' and motion['reopenByteExact'];template=json.loads((r.parent/'20261101-082926-joint-playable-display-c26c1b/route-release/summary.json').read_bytes())['command'];env=os.environ.copy();env['DOTNET_ROOT']='D:/Tools/dotnet9';env.pop('AZURERENDER_RESOURCE_ROOT',None);si=subprocess.STARTUPINFO();si.dwFlags|=subprocess.STARTF_USESHOWWINDOW;si.wShowWindow=0;report=dict(status='running',cases=[])
check=r/'lit-surfaces-playable-validation';check.mkdir();cmd=[str(root/'build/ninja-msvc-release/AzurePlayer.exe'),'--project',str(p),'--check-project']
with (check/'player.log').open('wb') as log:subprocess.run(cmd,cwd=check,env=env,startupinfo=si,stdout=log,stderr=log,check=True,timeout=900)
for cfg in ['release','debug']:
 name='lit-surfaces-game-'+cfg;out=r/name;out.mkdir();cmd=list(template);cmd[0]=str(root/('build/ninja-msvc-'+cfg)/'AzurePlayer.exe')
 for key,value in [('--project',p),('--runtime-report',out/'runtime.json'),('--capture-dir',out/'frames')]:cmd[cmd.index(key)+1]=str(value)
 (out/'command.json').write_text(json.dumps(cmd,indent=2));print('START',cfg,'540',flush=True)
 with (out/'player.log').open('wb') as log:subprocess.run(cmd,cwd=out,env=env,stdout=log,stderr=log,startupinfo=si,check=True,timeout=2400)
 log=(out/'player.log').read_text(errors='replace');assert 'Allocator after unload: buffers=0 images=0' in log and not re.search(r'VUID-|SYNC-HAZARD|Validation Error|\[error\]',log);subprocess.run([sys.executable,'-X','utf8',str(r/'verify_playable_route.py'),'--name',name],check=True);result=json.loads((out/'behavior-verification.json').read_bytes());report['cases'].append(dict(configuration=cfg,command=cmd,behavior=result,playerSha256=hashlib.sha256(Path(cmd[0]).read_bytes()).hexdigest()));(r/'lit-surfaces-playable-results.json').write_text(json.dumps(report,indent=2));print('PASS',cfg,flush=True)
report['status']='passed';(r/'lit-surfaces-playable-results.json').write_text(json.dumps(report,indent=2))
