import hashlib,json,re,subprocess,sys
from pathlib import Path
root=Path.cwd();run=Path(__file__).resolve().parent;family=run/'shader-family-revision';boundary=run/'shader-directory-revision'
read=lambda p:json.loads(p.read_text());sha=lambda p:hashlib.sha256(p.read_bytes()).hexdigest()
assert read(family/'pipeline.json')['status']=='passed'
assert read(boundary/'summary.json')['status']=='passed'
frames=instances=0
def log_ok(path):
    rows=[json.loads(line) for line in path.read_text().splitlines()]
    assert not any(r['level']=='error' or r['subsystem']=='validation' and r['level']=='warning' for r in rows),str(path)
    assert any(r['message']=='Allocator after unload: buffers=0 images=0' for r in rows),str(path)
reference=read(root/'build/runs/20261101-043757-material-display-transform-93d6e0/release-final-build.json')['shaders']
for cfg in ['release','debug']:
    old=read(family/'frozen-complete'/(cfg+'-build.json'));current=read(run/(cfg+'-build.json'))
    assert old['status']==current['status']=='built-contract-tests-passed' and len(old['tests'])==len(current['tests'])==17
    assert current['shaders']==old['shaders'] and all(current['shaders'].get(n)==d for n,d in reference.items())
    assert len(set(current['shaders'])-set(reference))==96
    changed={n for n,d in old['sourceFiles'].items() if current['sourceFiles'].get(n)!=d}
    assert changed=={'src/render/MaterialPipelinePolicy.hpp','src/scenes/CharacterMaterialSampling.inc','tests/MaterialPipelinePolicyTests.cpp'},changed
    for name,digest in current['sourceFiles'].items():assert hashlib.sha256((root/name).read_bytes().replace(b'\r\n',b'\n')).hexdigest()==digest,name
    for name,digest in current['toolFiles'].items():assert sha(root/name)==digest,name
    player=root/('build/ninja-msvc-'+cfg+'/AzurePlayer.exe');assert sha(player)==current['programs']['AzurePlayer']
    assert {p.name:sha(p) for p in (player.parent/'shaders').glob('*.spv')}==current['shaders']
    public=read(family/'public-gpu'/cfg/'summary.json');assert public['status']=='passed' and len(public['cases'])==84
    assert public['playerSha256']==old['programs']['AzurePlayer'] and public['shaders']==old['shaders']
    for case in public['cases']:
        folder=family/'public-gpu'/cfg/case['name'];assert case['status']=='passed' and case['exitCode']==0
        assert case['validationEnabled']==(cfg=='debug') and case.get('maximumLinearError',0)<=.008
        assert sha(folder/'frames/frame_000000.png')==case['imageSha256'];log_ok(folder/'captures/azureplayer.log.jsonl')
        frames+=1;instances+=1
    related=read(family/('related-'+cfg+'.json'));assert related['status']=='passed' and len(related['checks'])==7
    assert related['programSha256']==old['programs']['AzurePlayer']
    for check in related['checks']:
        assert check['status']=='passed';audit=read(family/'related-gpu'/cfg/(check['name']+'-audit.json'));assert audit['status']=='passed'
        for row in audit['players']:
            assert row['released'] and row['exitCode']==0;path=Path(row['structuredLog']);assert sha(path)==row['structuredLogSha256'];log_ok(path)
            frames+=int(row['command'][row['command'].index('--capture-frames')+1]);instances+=1
    route=family/('route-'+cfg);receipt=read(route/'summary.json');assert receipt['status']=='behavior-passed-visual-review-pending'
    assert receipt['programSha256']==old['programs']['AzurePlayer'] and receipt['shaderSha256']==old['shaders']
    assert read(route/'behavior-verification.json')['status']=='passed' and len(list((route/'frames').glob('frame_*.png')))==540
    log_ok(route/'captures/azureplayer.log.jsonl');frames+=540;instances+=1
    control=family/('directory-boundary-'+cfg+'-green');receipt=read(control/'receipt.json');assert receipt['status']=='passed'
    assert receipt['programSha256']==current['programs']['AzurePlayer']
    for case in read(control/'captures/summary.json')['cases']:
        assert case['status']=='passed' and case['exitCode']==0;log_ok(control/'captures'/case['name']/'captures/azureplayer.log.jsonl')
        frames+=1;instances+=1
native=read(family/'native-capture-summary.json');assert native['status']=='captured-inputs-unchanged' and len(native['cases'])==8
for case in native['cases']:
    assert case['status']=='captured' and case['exitCode']==0;folder=family/'native-captures'/case['name']
    for name,digest in case['images'].items():assert sha(folder/'frames'/name)==digest
    log_ok(folder/'captures/azureplayer.log.jsonl');frames+=len(case['images']);instances+=1
for case in read(boundary/'summary.json')['captures']:
    assert case['status']=='passed';folder=boundary/case['name']
    for name,digest in case['images'].items():assert sha(folder/'frames'/name)==digest
    log_ok(folder/'captures/azureplayer.log.jsonl');frames+=len(case['images']);instances+=1
compat=read(family/'compatibility-comparison.json');assert compat['strictCompatibilityPassed'] and compat['headCriteriaPassed']
for aa in [0,3]:
    data=read(family/f'body-comparison-aa{aa}.json');matched=[r for c in data['cases'] if c['pair']=='world-vs-matched-standalone' for r in c['materials'] if r['coverage']=='measured']
    assert len(matched)==18 and all(r['passed'] for r in matched)
    assert any(not r['passed'] for c in data['cases'] if c['pair']=='world-vs-original-standalone' for r in c['materials'] if r['coverage']=='measured')
assert read(family/'directory-boundary-release-red/receipt.json')['status']=='red-confirmed'
for name,digest in read(root/'build/runs/20261031-053326-startup-context-d7dce6/preserved-user-files.json').items():assert sha(root/name)==digest,name
assert frames==2061 and instances==496,(frames,instances)
docs=['README.md','CHANGELOG.md','docs/plans/azure-engine-plan.md','docs/plans/2026-11-01-material-environment-reception.md','docs/runtime/material-lighting-reception.md','docs/runtime/material-display-transform.md','docs/runtime/character-material-profile.md','docs/acceptance/nr/material-lighting-reception.md']
subprocess.run([sys.executable,'tools/check_doc_style.py',*docs],check=True)
report=dict(status='passed',scope='Public material lighting mechanism; complete regression and filename-boundary follow-up have separate frozen builds.',
    visualAcceptance='failed-user-review',capturedFrames=frames,successfulPlayerInstances=instances,compiledSourceFiles=989,cpuChecksPerConfiguration=17,
    defaultCompatiblePrograms=213,isolatedLightingPrograms=96,preservedUserFiles=4)
(run/'verification.json').write_text(json.dumps(report,indent=2)+'\n');print(json.dumps(report))
