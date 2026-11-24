import hashlib,json,re,subprocess,sys
from pathlib import Path
root=Path.cwd();run=Path(__file__).resolve().parent;read=lambda p:json.loads(p.read_text());sha=lambda p:hashlib.sha256(p.read_bytes()).hexdigest();total=0;instances=0
assert read(run/'regression-pipeline.json')['status']=='passed'
assert read(run/'revision-comparison.json')['status']=='passed'
def check_log(path):
    rows=[json.loads(line) for line in path.read_text().splitlines()]
    assert not any(r['level']=='error' or r['subsystem']=='validation' and r['level']=='warning' for r in rows),str(path)
    assert any(r['message']=='Allocator after unload: buffers=0 images=0' for r in rows),str(path)
def check_stdout(path):assert not re.search(r'VUID-|Validation (?:Error|Warning)|SYNC-HAZARD|\[error\]',path.read_text(errors='replace'),re.I),str(path)
for cfg in ['release','debug']:
    b=read(run/(cfg+'-build.json'));player=root/('build/ninja-msvc-'+cfg)/'AzurePlayer.exe';assert b['status']=='built-contract-tests-passed' and len(b['tests'])==17
    assert sha(player)==b['programs']['AzurePlayer']
    assert {p.name:sha(p) for p in (player.parent/'shaders').glob('*.spv')}==b['shaders']
    for name,digest in b['sourceFiles'].items():assert hashlib.sha256((root/name).read_bytes().replace(b'\r\n',b'\n')).hexdigest()==digest,name
    for name,digest in b['toolFiles'].items():assert sha(root/name)==digest,name
    s=read(run/'public-gpu'/cfg/'summary.json');assert s['status']=='passed' and len(s['cases'])==84 and s['shaders']==b['shaders'] and s['playerSha256']==sha(player)
    for case in s['cases']:
        folder=run/'public-gpu'/cfg/case['name'];assert case['status']=='passed' and case['exitCode']==0 and case['validationEnabled']==(cfg=='debug')
        assert sha(folder/'frames/frame_000000.png')==case['imageSha256'];assert case.get('maximumLinearError',0)<=.008
        for name,digest in case['inputs'].items():assert sha(folder/name)==digest,name
        check_log(folder/'captures/azureplayer.log.jsonl');check_stdout(folder/'player.log');total+=1;instances+=1
    related=read(run/('related-'+cfg+'.json'));assert related['status']=='passed' and len(related['checks'])==7 and related['programSha256']==sha(player)
    for check in related['checks']:
        assert check['status']=='passed' and check['exitCode']==0
        audit=read(run/'related-gpu'/cfg/(check['name']+'-audit.json'));assert audit['status']=='passed' and audit['toolSha256']==check['scriptSha256']
        for player_row in audit['players']:
            assert player_row['released'] and player_row['exitCode']==0;path=Path(player_row['structuredLog']);assert sha(path)==player_row['structuredLogSha256'];check_log(path)
            command=player_row['command'];folder=Path(command[command.index('--capture-dir')+1]);manifest=read(folder/'capture_manifest.json');assert manifest['materialSurfaceProfileLayoutVersion']==20
            frames=int(command[command.index('--capture-frames')+1]);assert len(list(folder.glob('frame_*.png')))==frames;total+=frames;instances+=1
    route=run/('route-'+cfg);r=read(route/'summary.json');assert r['status']=='behavior-passed-visual-review-pending' and r['exitCode']==0 and r['programSha256']==sha(player) and r['shaderSha256']==b['shaders']
    assert read(route/'behavior-verification.json')['status']=='passed';assert len(list((route/'frames').glob('frame_*.png')))==540;check_log(route/'captures/azureplayer.log.jsonl');check_stdout(route/'player.log');total+=540;instances+=1
native=read(run/'native-capture-summary.json');assert native['status']=='captured-inputs-unchanged' and len(native['cases'])==8 and native['shaders']==read(run/'release-build.json')['shaders']
for case in native['cases']:
    assert case['status']=='captured' and case['exitCode']==0;folder=run/'native-captures'/case['name']
    for name,digest in case['images'].items():assert sha(folder/'frames'/name)==digest,name
    check_log(folder/'captures/azureplayer.log.jsonl');check_stdout(folder/'player.log');total+=len(case['images']);instances+=1
assert sum(len(c['images']) for c in native['cases'])==282
for aa in [0,3]:
    c=read(run/f'body-comparison-aa{aa}.json');assert c['visualAcceptance']=='failed-user-review' and len(c['cases'])==9
    for row in c['cases']:
        measured=[r for r in row['materials'] if r['coverage']=='measured'];assert len(measured)==6
        if row['pair']=='world-vs-matched-standalone':assert all(r['passed'] for r in measured),(aa,row['frame'])
    assert any(not r['passed'] for row in c['cases'] if row['pair']=='world-vs-original-standalone' for r in row['materials'] if r['coverage']=='measured'),'Original frozen reference failures must remain visible'
compat=read(run/'compatibility-comparison.json');assert compat['strictCompatibilityPassed'] and compat['headCriteriaPassed']
for spec_name,folder in [('candidate.json','project'),('lighting-candidate.json','candidate-project')]:
    spec=read(run/spec_name)
    for name,digest in spec['inputs'].items():assert sha(run/folder/name)==digest,name
for name in ['red-cpu.json','red-public.json','probe-coupling-red.json']:assert read(run/name)['status']=='red-confirmed'
for cfg in ['release','debug']:
    b=read(run/'before-field'/(cfg+'-build.json'));s=read(run/'before-field/public-gpu'/cfg/'summary.json');assert s['status']=='passed' and len(s['cases'])==76 and s['shaders']==b['shaders']
assert read(run/'probe-coupling-red.json')['shaders']==read(run/'before-field/release-build.json')['shaders']
protected=read(root/'build/runs/20261031-053326-startup-context-d7dce6/preserved-user-files.json')
for name,digest in protected.items():assert sha(root/name)==digest,name
docs=['README.md','CHANGELOG.md','docs/plans/azure-engine-plan.md','docs/plans/2026-11-01-material-environment-reception.md','docs/runtime/material-lighting-reception.md','docs/runtime/material-display-transform.md','docs/runtime/character-material-profile.md','docs/acceptance/nr/material-lighting-reception.md']
subprocess.run([sys.executable,'tools/check_doc_style.py',*docs],check=True)
for name in docs:
    p=root/name
    for target in re.findall(r'\]\(([^)]+)\)',p.read_text()):
        if re.match(r'^[A-Za-z]+:',target) or target.startswith('#'):continue
        assert (p.parent/target.split('#')[0].strip('<>')).exists(),(name,target)
archive=root/'docs/acceptance/nr/evidence/2026-11-01/material-lighting-reception';m=read(archive/'manifest.json')
for row in m['files']:assert sha(archive/row['path'])==row['sha256'],row['path']
for row in m['reusedEvidence']:assert sha(root/row['path'])==row['sha256'],row['path']
result=dict(status='passed',visualAcceptance='failed-user-review',latestRevisionCapturedFrames=total,latestRevisionSuccessfulPlayerInstances=instances,compiledSourceFiles=989,cpuChecksPerConfiguration=17,publicLightingCapturesPerConfiguration=84,nativeCaptures=8,nativeFrames=282,routeFramesPerConfiguration=540,preservedUserFiles=4)
(run/'verification.json').write_bytes((json.dumps(result,indent=2)+'\n').encode());print(json.dumps(result))
