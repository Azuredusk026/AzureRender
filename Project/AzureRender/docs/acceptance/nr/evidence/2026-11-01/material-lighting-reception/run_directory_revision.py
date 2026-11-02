import hashlib,json,os,re,subprocess,sys,time
from pathlib import Path
run=Path(__file__).resolve().parent;root=run.parents[2];out=run/'shader-directory-revision';out.mkdir(exist_ok=False)
sha=lambda p:hashlib.sha256(p.read_bytes()).hexdigest();report=dict(status='running',checks=[],captures=[])
def save():(out/'summary.json').write_text(json.dumps(report,indent=2)+'\n')
save()
try:
    for cfg in ['release','debug']:
        build=json.loads((run/(cfg+'-build.json')).read_text());assert build['status']=='built-contract-tests-passed'
        (out/(cfg+'-build.json')).write_bytes((run/(cfg+'-build.json')).read_bytes())
        print('START boundary',cfg,flush=True)
        with (out/(cfg+'-boundary.log')).open('wb') as log:
            result=subprocess.run([sys.executable,'-X','utf8',str(run/'test_shader_directory_boundary.py'),'--expect','green','--configuration',cfg],stdout=log,stderr=log,cwd=root,timeout=600)
        assert result.returncode==0,cfg
        receipt=json.loads((run/'shader-family-revision'/('directory-boundary-'+cfg+'-green/receipt.json')).read_text());assert receipt['status']=='passed'
        report['checks'].append(dict(name='directory-'+cfg,status='passed',playerSha256=build['programs']['AzurePlayer']));save()
        print('DONE boundary',cfg,flush=True)
    player=root/'build/ninja-msvc-release/AzurePlayer.exe';env=os.environ.copy();env['DOTNET_ROOT']='D:/Tools/dotnet9'
    startup=subprocess.STARTUPINFO();startup.dwFlags|=subprocess.STARTF_USESHOWWINDOW;startup.wShowWindow=0
    reference=root/'build/runs/20261101-043757-material-display-transform-93d6e0/native-final-captures'
    for name,count,width,height in [('full-world-hidden',12,2560,1440),('near-default-compatibility',1,1536,1536)]:
        folder=out/name;folder.mkdir();command=[str(player),'--project',str(run/'candidate-project'/(name+'.azureproject')),'--width',str(width),'--height',str(height),
            '--capture-frames',str(count),'--capture-fps','60','--fixed-frame-step','--qa-camera','scene','--qa-light','scene','--capture-dir',str(folder/'frames'),'--runtime-report',str(folder/'runtime.json')]
        row=dict(name=name,status='running',command=command);report['captures'].append(row);save();print('START compatibility',name,flush=True)
        with (folder/'player.log').open('wb') as log:result=subprocess.run(command,cwd=folder,env=env,startupinfo=startup,stdout=log,stderr=log,timeout=700)
        assert result.returncode==0,name
        text=(folder/'player.log').read_text(errors='replace');assert not re.search(r'VUID-|Validation (?:Error|Warning)|SYNC-HAZARD|\[error\]',text)
        logs=[json.loads(line) for line in (folder/'captures/azureplayer.log.jsonl').read_text().splitlines()]
        assert not any(r['level']=='error' or r['subsystem']=='validation' and r['level']=='warning' for r in logs)
        assert any(r['message']=='Allocator after unload: buffers=0 images=0' for r in logs)
        images={p.name:sha(p) for p in sorted((folder/'frames').glob('frame_*.png'))};assert len(images)==count
        assert all(digest==sha(reference/name/'frames'/file) for file,digest in images.items()),name
        row.update(status='passed',images=images,exitCode=result.returncode);save();print('DONE compatibility',name,flush=True)
    for cfg in ['release','debug']:
        build=json.loads((out/(cfg+'-build.json')).read_text());assert sha(root/('build/ninja-msvc-'+cfg+'/AzurePlayer.exe'))==build['programs']['AzurePlayer']
        for name,digest in build['sourceFiles'].items():assert hashlib.sha256((root/name).read_bytes().replace(b'\r\n',b'\n')).hexdigest()==digest,name
    report.update(status='passed',sourceScope='Complete regression uses the frozen prior revision. This follow-up changes only filename-scoped program classification and adds its regressions.',successfulPlayerInstances=10,capturedFrames=21);save()
except Exception as error:report.update(status='failed',error=repr(error));save();raise
