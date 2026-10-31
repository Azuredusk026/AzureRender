import argparse,hashlib,json,os,re,subprocess,time
from pathlib import Path
root=Path.cwd();run=Path(__file__).resolve().parent;player=root/'build/ninja-msvc-release/AzurePlayer.exe'
parser=argparse.ArgumentParser();parser.add_argument('--extra',action='store_true');args=parser.parse_args()
env=os.environ.copy();env['DOTNET_ROOT']='D:/Tools/dotnet9';env['PYTHONUTF8']='1'
startup=subprocess.STARTUPINFO();startup.dwFlags|=subprocess.STARTF_USESHOWWINDOW;startup.wShowWindow=0
report={'programSha256':hashlib.sha256(player.read_bytes()).hexdigest(),'visualAcceptance':'pending','cases':[]}
try:
    for case in json.loads((run/('material-ab-extra-cases.json' if args.extra else 'material-ab-cases.json')).read_text()):
        if case['name']=='reference':continue
        out=run/'material-capture-3'/case['name'];out.mkdir(parents=True)
        command=[str(player),'--project',case['project'],'--width','1280','--height','720','--capture-frames','32','--fixed-frame-step','--capture-dir',str(out/'frames'),'--qa-camera','scene','--qa-light','scene','--qa-isolation','beauty','--set','render.antiAliasing=3','--runtime-report',str(out/'runtime.json')]
        row={**case,'command':command};report['cases'].append(row);print('START',case['name'],flush=True);start=time.perf_counter()
        with (out/'player.log').open('wb') as log:result=subprocess.run(command,stdout=log,stderr=log,env=env,startupinfo=startup,timeout=700)
        row.update(exitCode=result.returncode,seconds=time.perf_counter()-start)
        assert result.returncode==0,row
        text=(out/'player.log').read_text(errors='replace');assert not re.search(r'VUID-|Validation (?:Error|Warning)|SYNC-HAZARD|\[error\]',text,re.I)
        assert 'Allocator after unload: buffers=0 images=0' in text
        row['manifest']=json.loads((out/'frames/capture_manifest.json').read_text());assert row['manifest']['capturedFrames']==32
        assert hashlib.sha256(player.read_bytes()).hexdigest()==report['programSha256']
        print('DONE',case['name'],row['seconds'],flush=True)
    report['status']='captured'
finally:(run/('material-ab-extra-capture.json' if args.extra else 'material-ab-capture.json')).write_text(json.dumps(report,indent=2)+'\n')
