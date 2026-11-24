import hashlib,json,os,re,subprocess,time
from pathlib import Path
root=Path.cwd();run=Path(__file__).resolve().parent
inputs=json.loads((run/'native-inputs.json').read_text());project=Path(inputs['project'])
player=root/'build/ninja-msvc-release/AzurePlayer.exe'
sha=lambda p:hashlib.sha256(p.read_bytes()).hexdigest()
files=[project/'content-character/hero-restored-inputs-v8.glb']
files += [p for folder in ['assets/restored-character-materials','assets/normalized-character-materials','assets/linear-display-materials','native-gi'] for p in (project/folder).glob('*') if p.is_file()]
files += [Path(case[key]) for case in inputs['cases'] for key in ['project','level']]
frozen={str(p.relative_to(root)):sha(p) for p in files}
shaders={p.name:sha(p) for p in sorted((player.parent/'shaders').glob('*.spv'))}
build=json.loads((run/'release-final-build.json').read_text());assert build['programs']['AzurePlayer']==sha(player) and build['shaders']==shaders
env=os.environ.copy();env['DOTNET_ROOT']='D:/Tools/dotnet9';env['PYTHONUTF8']='1'
startup=subprocess.STARTUPINFO();startup.dwFlags|=subprocess.STARTF_USESHOWWINDOW;startup.wShowWindow=0
report={'status':'running','programSha256':sha(player),'shaderSha256':shaders,'inputs':frozen,'visualAcceptance':'failed-user-review','cases':[]}
try:
    for case in inputs['cases']:
        out=run/'native-final-captures'/case['name'];out.mkdir(parents=True)
        command=[str(player),'--project',case['project'],'--width',str(case['width']),'--height',str(case['height']),
            '--capture-frames',str(case['captureFrames']),'--fixed-frame-step','--capture-fps','60','--capture-dir',str(out/'frames'),
            '--qa-camera','scene','--qa-light','scene','--runtime-report',str(out/'runtime.json')]
        row={'name':case['name'],'command':command,'status':'running'};report['cases'].append(row)
        (out/'command.json').write_text(json.dumps(row,indent=2)+'\n');print('START',case['name'],flush=True);start=time.perf_counter()
        with (out/'player.log').open('wb') as log:result=subprocess.run(command,stdout=log,stderr=log,env=env,startupinfo=startup,timeout=700,cwd=out)
        row.update(exitCode=result.returncode,seconds=time.perf_counter()-start);assert result.returncode==0,row
        log=(out/'player.log').read_text(errors='replace')
        assert not re.search(r'VUID-|Validation (?:Error|Warning)|SYNC-HAZARD|\[error\]',log,re.I),case['name']
        diagnostics=[json.loads(line) for line in (out/'captures/azureplayer.log.jsonl').read_text().splitlines()]
        assert not any(item['level']=='error' for item in diagnostics)
        assert any(item['message']=='Allocator after unload: buffers=0 images=0' for item in diagnostics)
        row['allocatorUnloadVerified']=True
        manifest=json.loads((out/'frames/capture_manifest.json').read_text())
        assert manifest['capturedFrames']==case['captureFrames'] and manifest['materialSurfaceProfileLayoutVersion']==19
        assert len(manifest['materialDisplayTransforms'])==(11 if case['normalized'] else 0)
        scales=manifest['materialRadianceScales'];assert len(scales)==(11 if case['normalized'] else 0),(case['name'],scales)
        assert all(x['radianceScale']==.25 for x in scales)
        runtime=json.loads((out/'runtime.json').read_text());assert runtime['scriptErrors']==[] and runtime['presentationErrors']==[]
        row.update(status='captured',manifest=manifest,images={p.name:sha(p) for p in sorted((out/'frames').glob('*.png'))})
        assert sha(player)==report['programSha256'];print('DONE',case['name'],row['seconds'],flush=True)
        (run/'native-final-capture-summary.json').write_text(json.dumps(report,indent=2)+'\n')
    assert shaders=={p.name:sha(p) for p in sorted((player.parent/'shaders').glob('*.spv'))}
    for name,digest in frozen.items():assert sha(root/name)==digest,name
    report['status']='captured-inputs-unchanged'
except Exception as error:report.update(status='failed',failure=str(error));raise
finally:(run/'native-final-capture-summary.json').write_text(json.dumps(report,indent=2)+'\n')
