import hashlib,json,os,re,subprocess,time
from pathlib import Path
root=Path.cwd();run=Path(__file__).resolve().parent;source=root/'build/runs/20261031-231334-receiver-directional-light-911ab7'
player=root/'build/ninja-msvc-release/AzurePlayer.exe';sha=lambda p:hashlib.sha256(p.read_bytes()).hexdigest()
inputs=[source/'project/content-character/hero-restored-inputs-v8.glb']+list((source/'project/assets/restored-character-materials').glob('*'))+list((source/'project/native-gi').glob('*'))
inputs=[p for p in inputs if p.is_file()];frozen={str(p.relative_to(root)):sha(p) for p in inputs}
projects={'joint':source/'project/near-control.azureproject','reference-look':source/'project/near-reference-source-display.azureproject'}
env=os.environ.copy();env['DOTNET_ROOT']='D:/Tools/dotnet9';env['PYTHONUTF8']='1'
startup=subprocess.STARTUPINFO();startup.dwFlags|=subprocess.STARTF_USESHOWWINDOW;startup.wShowWindow=0
report={'status':'running','sourceCommit':subprocess.check_output(['git','rev-parse','HEAD'],text=True).strip(),'programSha256':sha(player),'visualAcceptance':'failed-user-review','reopenedRun':source.name,'scope':'Same-pose component isolation; radiometric cases force -2 EV and disable grading through the existing QA contract. This measures lighting before the two beauty display transforms.','inputs':frozen,'cases':[]}
try:
    for isolation in ['ambient','direct-diffuse','hair-kk','object-shadow-visibility','face-sdf']:
        for label,project in projects.items():
            name=label+'-'+isolation;out=run/'captures'/name;out.mkdir(parents=True)
            command=[str(player),'--project',str(project),'--width','1536','--height','1536','--capture-frames','1','--fixed-frame-step','--capture-dir',str(out/'frames'),'--qa-camera','scene','--qa-light','scene','--qa-isolation',isolation,'--runtime-report',str(out/'runtime.json')]
            if isolation in ['ambient','direct-diffuse','hair-kk']:command.extend(['--qa-exposure','-2'])
            row={'name':name,'command':command,'status':'running'};report['cases'].append(row);(out/'command.json').write_text(json.dumps(row,indent=2)+'\n');print('START',name,flush=True);start=time.perf_counter()
            with (out/'player.log').open('wb') as log:result=subprocess.run(command,stdout=log,stderr=log,env=env,startupinfo=startup,timeout=700)
            row.update(exitCode=result.returncode,seconds=time.perf_counter()-start)
            assert result.returncode==0,row
            log=(out/'player.log').read_text(errors='replace');assert not re.search(r'VUID-|Validation (?:Error|Warning)|SYNC-HAZARD|\[error\]',log,re.I)
            assert 'Allocator after unload: buffers=0 images=0' in log
            manifest=json.loads((out/'frames/capture_manifest.json').read_text());assert manifest['capturedFrames']==1
            row.update(status='captured',manifest=manifest,imageSha256=sha(out/'frames/frame_000000.png'))
            assert sha(player)==report['programSha256'];print('DONE',name,row['seconds'],flush=True)
            (run/'capture-summary.json').write_text(json.dumps(report,indent=2)+'\n')
    for name,digest in frozen.items():assert sha(root/name)==digest,name
    report['status']='captured-inputs-unchanged'
finally:(run/'capture-summary.json').write_text(json.dumps(report,indent=2)+'\n')
