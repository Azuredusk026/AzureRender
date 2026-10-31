import argparse,hashlib,json,os,re,subprocess,time
from pathlib import Path
root=Path.cwd();run=Path(__file__).resolve().parent
parser=argparse.ArgumentParser();parser.add_argument('--case',action='append',required=True);args=parser.parse_args()
lookup={x['name']:x for x in json.loads((run/'character-cases.json').read_text())}
player=root/'build/ninja-msvc-release/AzurePlayer.exe';sha=lambda p:hashlib.sha256(p.read_bytes()).hexdigest()
report={'status':'running','programSha256':sha(player),'visualAcceptance':'pending','cases':[]}
env=os.environ.copy();env['DOTNET_ROOT']='D:/Tools/dotnet9'
startup=subprocess.STARTUPINFO();startup.dwFlags|=subprocess.STARTF_USESHOWWINDOW;startup.wShowWindow=0
try:
    for name in args.case:
        case=lookup[name];out=run/'character-capture'/name;out.mkdir(parents=True)
        near=name.startswith('near-');frames=case.get('captureFrames',1 if near else 12)
        command=[str(player),'--project',case['project'],'--width',str(1536 if near else 2560),'--height',str(1536 if near else 1440),'--capture-frames',str(frames),'--capture-fps','60','--fixed-frame-step','--capture-dir',str(out/'frames'),'--qa-camera','scene','--qa-light','scene','--runtime-report',str(out/'runtime.json')]
        row={**case,'command':command,'status':'running'};report['cases'].append(row)
        (out/'summary.json').write_text(json.dumps(row,indent=2)+'\n')
        print('START',name,flush=True);start=time.perf_counter()
        with (out/'player.log').open('wb') as log:
            result=subprocess.run(command,stdout=log,stderr=log,env=env,startupinfo=startup,timeout=700)
        row.update(exitCode=result.returncode,seconds=time.perf_counter()-start)
        assert result.returncode==0,name
        text=(out/'player.log').read_text(errors='replace')
        assert 'Allocator after unload: buffers=0 images=0' in text
        assert not re.search(r'VUID-|Validation (?:Error|Warning)|SYNC-HAZARD|\[error\]',text,re.I)
        rt=json.loads((out/'runtime.json').read_text());assert rt['scriptErrors']==[] and rt['presentationErrors']==[]
        row['capture']=json.loads((out/'frames/capture_manifest.json').read_text())
        assert row['capture']['capturedFrames']==frames and sha(player)==report['programSha256']
        row['status']='captured-visual-acceptance-pending';(out/'summary.json').write_text(json.dumps(row,indent=2)+'\n')
        print('DONE',name,row['seconds'],flush=True)
    report['status']='captured-visual-acceptance-pending'
finally:
    (run/('character-summary-'+'-'.join(args.case)+'.json')).write_text(json.dumps(report,indent=2)+'\n')
