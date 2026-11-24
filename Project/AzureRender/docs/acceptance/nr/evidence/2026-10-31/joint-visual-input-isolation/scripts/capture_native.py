from pathlib import Path
import hashlib,json,os,re,subprocess,time
root=Path.cwd(); run=Path(__file__).resolve().parent
player=root/'build/ninja-msvc-release/AzurePlayer.exe'
sha=lambda p:hashlib.sha256(p.read_bytes()).hexdigest()
report={'status':'running','programSha256':sha(player),'cases':[]}
env=os.environ.copy();env['DOTNET_ROOT']='D:/Tools/dotnet9'
startup=subprocess.STARTUPINFO();startup.dwFlags|=subprocess.STARTF_USESHOWWINDOW;startup.wShowWindow=0
try:
    for case in json.loads((run/'native-cases.json').read_text()):
        out=run/case['name'];out.mkdir()
        command=[str(player),'--project',case['project'],'--width','1536','--height','1536','--capture-frames','1','--capture-fps','60','--fixed-frame-step','--capture-dir',str(out/'frames'),'--qa-camera','scene','--qa-light','scene','--runtime-report',str(out/'runtime.json')]
        row={**case,'status':'running','command':command};report['cases'].append(row)
        (run/'native-summary.json').write_text(json.dumps(report,indent=2)+'\n')
        print('START',case['name'],flush=True);start=time.perf_counter()
        with (out/'player.log').open('wb') as log:
            result=subprocess.run(command,env=env,stdout=log,stderr=log,startupinfo=startup,timeout=600)
        row.update(exitCode=result.returncode,seconds=time.perf_counter()-start)
        assert result.returncode==0,case['name']
        text=(out/'player.log').read_text(errors='replace')
        assert 'Allocator after unload: buffers=0 images=0' in text
        assert not re.search(r'VUID-|Validation (?:Error|Warning)|SYNC-HAZARD|\[error\]',text,re.I)
        row['capture']=json.loads((out/'frames/capture_manifest.json').read_text())
        row['status']='captured-visual-acceptance-pending'
        print('DONE',case['name'],row['seconds'],flush=True)
    report['status']='captured-visual-acceptance-pending'
finally:
    (run/'native-summary.json').write_text(json.dumps(report,indent=2)+'\n')
