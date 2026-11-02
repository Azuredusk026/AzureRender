import argparse,hashlib,json,os,re,subprocess,time
from pathlib import Path
root=Path.cwd();run=Path(__file__).resolve().parent
parser=argparse.ArgumentParser();parser.add_argument('--config',choices=['release','debug'],required=True);args=parser.parse_args()
spec=json.loads((run/'route-spec.json').read_text());project=root/spec['project'];out=run/'shader-family-revision'/('route-'+args.config);out.mkdir()
sha=lambda p:hashlib.sha256(p.read_bytes()).hexdigest()
for name,digest in spec['inputs'].items():assert sha(project.parent/name)==digest,name
program=root/('build/ninja-msvc-'+args.config)/'AzurePlayer.exe'
command=[str(program),'--project',str(project),'--width','960','--height','540','--capture-fps','60','--fixed-frame-step','--qa-camera','scene','--qa-light','scene','--qa-isolation','beauty','--runtime-report',str(out/'runtime.json'),'--game-actions',str(run/'input-route.json'),'--capture-frames','540','--capture-dir',str(out/'frames')]
env=os.environ.copy();env['DOTNET_ROOT']='D:/Tools/dotnet9'
startup=subprocess.STARTUPINFO();startup.dwFlags|=subprocess.STARTF_USESHOWWINDOW;startup.wShowWindow=0
record=dict(status='running',command=command,programSha256=sha(program),shaderSha256={p.name:sha(p) for p in (program.parent/'shaders').glob('*.spv')},projectSha256=sha(project),actionsSha256=sha(run/'input-route.json'));start=time.perf_counter()
try:
 with (out/'player.log').open('wb') as log:process=subprocess.run(command,cwd=out,stdout=log,stderr=log,env=env,startupinfo=startup,timeout=1800)
 record.update(exitCode=process.returncode,elapsedSeconds=time.perf_counter()-start);assert process.returncode==0
 assert not re.search(r'VUID-|Validation (?:Error|Warning)|SYNC-HAZARD|\[error\]',(out/'player.log').read_text(errors='replace'),re.I)
 diagnostics=[json.loads(line) for line in (out/'captures/azureplayer.log.jsonl').read_text().splitlines()]
 assert not any(row['level']=='error' or row['subsystem']=='validation' and row['level']=='warning' for row in diagnostics)
 assert any('Allocator after unload: buffers=0 images=0' in str(row) for row in diagnostics)
 data=json.loads((out/'runtime.json').read_text());assert not data['scriptErrors'] and not data['presentationErrors']
 assert len(data['routeFrames'])==540 and data['replayedActions']==13
 manifest=json.loads((out/'frames/capture_manifest.json').read_text());assert manifest['capturedFrames']==540
 assert manifest['materialSurfaceProfileLayoutVersion']==20 and len(manifest['materialDisplayTransforms'])==11
 assert len(manifest['materialLightingReceivers'])==5
 assert len(manifest['materialRadianceScales'])==11 and all(item['radianceScale']==.25 for item in manifest['materialRadianceScales'])
 subprocess.run([str(root/'build/reinstall-environment/venv/Scripts/python.exe'),'-X','utf8',str(run/'verify_behavior.py'),'--name',str(out.relative_to(run))],cwd=root,check=True,stdout=subprocess.DEVNULL)
 for name,digest in spec['inputs'].items():assert sha(project.parent/name)==digest,name
 for name,digest in spec['frozenSourceFiles'].items():assert sha(root/name)==digest,name
 assert sha(program)==record['programSha256'] and {p.name:sha(p) for p in (program.parent/'shaders').glob('*.spv')}==record['shaderSha256'];record['status']='behavior-passed-visual-review-pending'
finally:
 if record['status']=='running':record['status']='failed'
 (out/'summary.json').write_bytes((json.dumps(record,indent=2)+'\n').encode());print(json.dumps({k:v for k,v in record.items() if k not in {'command','shaderSha256'}}))
