from pathlib import Path
import argparse,hashlib,json,os,re,subprocess,time
root=Path.cwd(); run=Path(__file__).resolve().parent
parser=argparse.ArgumentParser();parser.add_argument('--name',required=True);parser.add_argument('--config',choices=['debug','release'],default='release');parser.add_argument('--frames',type=int,default=540);parser.add_argument('--actions',type=Path);parser.add_argument('--smoke',action='store_true');args=parser.parse_args()
assert re.fullmatch('[a-z0-9-]+',args.name)
spec=json.loads((run/'candidate.json').read_text()); project=root/spec['project']
out=run/args.name;out.mkdir();actions=args.actions.resolve() if args.actions else run/'input-route.json'
command=[str(root/f'build/ninja-msvc-{args.config}/AzurePlayer.exe'),'--project',str(project),'--width','960','--height','540','--capture-fps','60','--fixed-frame-step','--qa-camera','scene','--qa-light','scene','--qa-isolation','beauty','--runtime-report',str(out/'runtime.json'),'--game-actions',str(actions)]
command+=['--smoke-frames',str(args.frames)] if args.smoke else ['--capture-frames',str(args.frames),'--capture-dir',str(out/'frames')]
sha=lambda p:hashlib.sha256(p.read_bytes()).hexdigest()
report=dict(status='running',command=command,programSha256=sha(Path(command[0])),projectSha256=sha(project),actionsSha256=sha(actions))
env=os.environ.copy();env['DOTNET_ROOT']='D:/Tools/dotnet9'
startup=subprocess.STARTUPINFO();startup.dwFlags|=subprocess.STARTF_USESHOWWINDOW;startup.wShowWindow=0
start=time.perf_counter()
try:
 with (out/'player.log').open('wb') as log:
  process=subprocess.run(command,stdout=log,stderr=log,env=env,startupinfo=startup,timeout=1200)
 report.update(exitCode=process.returncode,elapsedSeconds=time.perf_counter()-start)
 assert process.returncode==0
 log=(out/'player.log').read_text(errors='replace')
 assert 'Allocator after unload: buffers=0 images=0' in log
 assert not re.search(r'VUID-|Validation (?:Error|Warning)|SYNC-HAZARD|\[error\]',log,re.I)
 data=json.loads((out/'runtime.json').read_text())
 assert data['scriptErrors']==[] and data['presentationErrors']==[]
 assert data['replayedActions']==len(json.loads(actions.read_text())['actions']) and len(data['routeFrames'])==args.frames
 assert sha(Path(command[0]))==report['programSha256'] and sha(project)==report['projectSha256']
 report.update(status='captured-behavior-review-pending',routeFrames=len(data['routeFrames']),replayedActions=data['replayedActions'])
finally:
 if report['status']=='running':report['status']='failed'
 (out/'summary.json').write_bytes((json.dumps(report,indent=2)+'\n').encode())
 print(json.dumps(report,indent=2))
