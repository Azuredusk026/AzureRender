from pathlib import Path
import os,json,sys,subprocess,hashlib,re
r=Path(__file__).resolve().parent;root=r.parents[2];p=r/'qualified-lit-surfaces-joint-v4/joint-materials.azureproject';env=os.environ.copy();env['DOTNET_ROOT']='D:/Tools/dotnet9';si=subprocess.STARTUPINFO();si.dwFlags|=subprocess.STARTF_USESHOWWINDOW;si.wShowWindow=0
report=dict(status='running',cases=[])
for cfg,count in [('release',192),('debug',4)]:
 out=r/('lit-surfaces-joint-motion-'+cfg);out.mkdir();player=root/('build/ninja-msvc-'+cfg)/'AzurePlayer.exe';cmd=[str(player),'--project',str(p),'--width','2560','--height','1440','--qa-camera','scene','--qa-light','scene','--capture-frames',str(count),'--capture-fps','24','--fixed-frame-step','--capture-dir',str(out/'frames'),'--runtime-report',str(out/'runtime.json')];(out/'command.json').write_text(json.dumps(cmd,indent=2));print('START',cfg,count,flush=True)
 with (out/'player.log').open('wb') as log:subprocess.run(cmd,cwd=out,env=env,startupinfo=si,stdout=log,stderr=log,check=True,timeout=2100)
 runtime=json.loads((out/'runtime.json').read_bytes());assert runtime['animationFrames']==count*11 and len(runtime['animations'])==11 and any(v['node']=='hero' and v['clip']==9 for v in runtime['animations']);assert not runtime['scriptErrors'] and not runtime['presentationErrors'];log=(out/'player.log').read_text(errors='replace');assert 'Allocator after unload: buffers=0 images=0' in log and not re.search(r'VUID-|SYNC-HAZARD|Validation Error|\[error\]',log);frames={f.name:hashlib.sha256(f.read_bytes()).hexdigest() for f in sorted((out/'frames').glob('frame_*.png'))};assert len(frames)==count and len(set(frames.values()))==count
 if cfg=='release':release_frames=frames
 else:assert all(release_frames[name]==digest for name,digest in frames.items())
 report['cases'].append(dict(configuration=cfg,frames=count,animationNodeSamples=count*11,images=frames,status='passed',playerSha256=hashlib.sha256(player.read_bytes()).hexdigest()));(r/'lit-surfaces-joint-motion-results.json').write_text(json.dumps(report,indent=2));print('PASS',cfg,flush=True)
report['status']='passed';report['reopenByteExact']=True;(r/'lit-surfaces-joint-motion-results.json').write_text(json.dumps(report,indent=2))
