from pathlib import Path
import hashlib,json,os,re,subprocess,time
root=Path.cwd();r=Path(__file__).resolve().parent;out=r/'editor-save';out.mkdir();project=r/'source-project/playable.azureproject';player=r/'engine/bin/AzureRender.exe'
command=[str(player),'--editor-project',str(project),'--editor-actions',str(r/'editor-actions.json'),'--editor-close-policy','save','--width','960','--height','540','--fixed-frame-step','--smoke-frames','8','--runtime-report',str(out/'runtime.json')]
sha=lambda p:hashlib.sha256(p.read_bytes()).hexdigest();report=dict(status='running',command=command,programSha256=sha(player),projectSha256=sha(project),levelBeforeSha256=sha(project.parent/'scene/playable.azurelevel'));env=os.environ.copy();env['DOTNET_ROOT']='D:/Tools/dotnet9';startup=subprocess.STARTUPINFO();startup.dwFlags|=subprocess.STARTF_USESHOWWINDOW;startup.wShowWindow=0;t=time.perf_counter()
try:
 with (out/'editor.log').open('wb') as log:p=subprocess.run(command,stdout=log,stderr=log,env=env,startupinfo=startup,timeout=900)
 report.update(exitCode=p.returncode,elapsedSeconds=time.perf_counter()-t);assert p.returncode==0
 log=(out/'editor.log').read_text(errors='replace');assert 'Allocator after unload: buffers=0 images=0' in log;assert not re.search(r'VUID-|Validation (?:Error|Warning)|SYNC-HAZARD|\[error\]',log,re.I)
 d=json.loads((out/'runtime.json').read_text());assert len(d['editorActions'])==1 and d['editorActions'][0]['command']=='save' and d['editorActions'][0]['passed']
 report.update(status='saved-semantic-review-pending',editorActions=d['editorActions'],levelAfterSha256=sha(project.parent/'scene/playable.azurelevel'));assert sha(player)==report['programSha256']
finally:
 if report['status']=='running':report['status']='failed'
 (out/'summary.json').write_bytes((json.dumps(report,indent=2)+'\n').encode());print(json.dumps(report,indent=2))
