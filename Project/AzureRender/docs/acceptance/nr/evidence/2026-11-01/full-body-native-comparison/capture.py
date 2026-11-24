import argparse,hashlib,json,os,re,subprocess,time
from pathlib import Path
root=Path.cwd();run=Path(__file__).resolve().parent;parser=argparse.ArgumentParser();group=parser.add_mutually_exclusive_group();group.add_argument('--diagnostics',action='store_true');group.add_argument('--masks',action='store_true');args=parser.parse_args();spec=json.loads((run/'candidate.json').read_text());input_root=run/'project';output_name='captures';report_name='capture-summary.json'
if args.diagnostics:
 diagnostic=json.loads((run/'diagnostic-inputs.json').read_text());spec=dict(spec,cases=diagnostic['cases'],inputs=diagnostic['inputs']);input_root=run/diagnostic['projectRoot'];output_name='diagnostic-captures';report_name='diagnostic-capture-summary.json'
if args.masks:
 masks=json.loads((run/'mask-inputs.json').read_text());spec=dict(spec,cases=[dict(name='material-ids',project=masks['project'],width=1536,height=1536,frames=64)],inputs=masks['inputs']);input_root=run/'mask-project';output_name='mask-captures';report_name='mask-capture-summary.json'
player=root/'build/ninja-msvc-release/AzurePlayer.exe';sha=lambda p:hashlib.sha256(p.read_bytes()).hexdigest();assert sha(player)==spec['programSha256'];report=dict(status='running',visualAcceptance='failed-user-review',programSha256=sha(player),cases=[]);env=os.environ.copy();env['DOTNET_ROOT']='D:/Tools/dotnet9';startup=subprocess.STARTUPINFO();startup.dwFlags|=subprocess.STARTF_USESHOWWINDOW;startup.wShowWindow=0
try:
 for case in spec['cases']:
  out=run/output_name/case['name'];out.mkdir(parents=True);command=[str(player),'--project',case['project'],'--width',str(case['width']),'--height',str(case['height']),'--capture-frames',str(case['frames']),'--fixed-frame-step','--capture-fps','60','--qa-animation','--capture-dir',str(out/'frames'),'--qa-camera','scene','--qa-light','scene','--runtime-report',str(out/'runtime.json')];row=dict(name=case['name'],command=command,status='running');report['cases'].append(row);start=time.perf_counter();print('START',case['name'],flush=True)
  with (out/'player.log').open('wb') as log:result=subprocess.run(command,cwd=out,stdout=log,stderr=log,env=env,startupinfo=startup,timeout=700)
  row.update(exitCode=result.returncode,seconds=time.perf_counter()-start);assert result.returncode==0,case['name'];assert not re.search(r'VUID-|Validation (?:Error|Warning)|SYNC-HAZARD|\[error\]',(out/'player.log').read_text(errors='replace'),re.I)
  logs=[json.loads(line) for line in (out/'captures/azureplayer.log.jsonl').read_text().splitlines()];assert not any(r['level']=='error' or r['subsystem']=='validation' and r['level']=='warning' for r in logs);assert any(r['message']=='Allocator after unload: buffers=0 images=0' for r in logs)
  manifest=json.loads((out/'frames/capture_manifest.json').read_text());assert manifest['capturedFrames']==case['frames'] and manifest['materialSurfaceProfileLayoutVersion']==19 and len(manifest['materialDisplayTransforms'])==(13 if args.masks else 11) and manifest['qaAnimation'];runtime=json.loads((out/'runtime.json').read_text());assert not runtime['scriptErrors'] and not runtime['presentationErrors']
  images={p.name:sha(p) for p in sorted((out/'frames').glob('frame_*.png'))};assert len(images)==case['frames'];row.update(status='captured',images=images,manifestSha256=sha(out/'frames/capture_manifest.json'));print('DONE',case['name'],round(row['seconds'],2),flush=True);(run/report_name).write_bytes((json.dumps(report,indent=2)+'\n').encode())
 for name,digest in spec['inputs'].items():assert sha(input_root/name)==digest,name
 assert sha(player)==spec['programSha256'];assert {p.name:sha(p) for p in (player.parent/'shaders').glob('*.spv')}==spec['shaders'];report['status']='captured-inputs-unchanged'
finally:
 if report['status']=='running':report['status']='failed'
 (run/report_name).write_bytes((json.dumps(report,indent=2)+'\n').encode());print(report['status'])
