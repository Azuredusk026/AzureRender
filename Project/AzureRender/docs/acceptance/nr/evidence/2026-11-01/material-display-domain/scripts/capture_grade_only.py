import hashlib,json,os,re,subprocess,time
from pathlib import Path
root=Path.cwd();run=Path(__file__).resolve().parent
inputs=json.loads((run/'grade-only-inputs.json').read_text());player=root/'build/ninja-msvc-release/AzurePlayer.exe'
source_run=root/'build/runs/20261101-021345-material-radiance-normalization-1e6231'
assert json.loads((source_run/'related-gpu-summary.json').read_text())['status']=='passed'
sha=lambda p:hashlib.sha256(p.read_bytes()).hexdigest()
shaders={p.name:sha(p) for p in sorted((player.parent/'shaders').glob('*.spv'))};program=sha(player)
env=os.environ.copy();env.update(DOTNET_ROOT='D:/Tools/dotnet9',PYTHONUTF8='1')
out=run/'grade-only-capture';out.mkdir();command=[str(player),'--project',inputs['project'],'--width','1536','--height','1536','--capture-frames','1',
    '--fixed-frame-step','--capture-fps','60','--capture-dir',str(out/'frames'),'--qa-camera','scene','--qa-light','scene','--runtime-report',str(out/'runtime.json')]
report={'status':'running','programSha256':program,'shaderSha256':shaders,'command':command,'visualAcceptance':'failed-user-review'}
(out/'command.json').write_text(json.dumps(command,indent=2)+'\n');start=time.perf_counter()
try:
    with (out/'player.log').open('wb') as log:result=subprocess.run(command,stdout=log,stderr=log,env=env,timeout=700,cwd=out)
    report.update(exitCode=result.returncode,seconds=time.perf_counter()-start);assert result.returncode==0
    log=(out/'player.log').read_text(errors='replace');assert not re.search(r'VUID-|Validation (?:Error|Warning)|SYNC-HAZARD|\[error\]',log,re.I)
    diagnostics=[json.loads(line) for line in (out/'captures/azureplayer.log.jsonl').read_text().splitlines()]
    assert not any(row['level']=='error' for row in diagnostics)
    assert any(row['message']=='Allocator after unload: buffers=0 images=0' for row in diagnostics)
    runtime=json.loads((out/'runtime.json').read_text());assert runtime['scriptErrors']==[] and runtime['presentationErrors']==[]
    manifest=json.loads((out/'frames/capture_manifest.json').read_text());assert manifest['capturedFrames']==1 and manifest['materialSurfaceProfileLayoutVersion']==18
    for name,digest in inputs['inputs'].items():assert sha(root/name)==digest,name
    assert sha(player)==program and shaders=={p.name:sha(p) for p in sorted((player.parent/'shaders').glob('*.spv'))}
    report.update(status='captured-inputs-unchanged',manifest=manifest,imageSha256=sha(out/'frames/frame_000000.png'))
except Exception as error:report.update(status='failed',failure=str(error));raise
finally:(run/'grade-only-capture.json').write_text(json.dumps(report,indent=2)+'\n')
print(json.dumps({'status':report['status'],'seconds':report['seconds']}))
