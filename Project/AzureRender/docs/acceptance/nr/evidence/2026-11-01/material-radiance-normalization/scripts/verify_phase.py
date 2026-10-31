import hashlib,json,re,subprocess,sys
from pathlib import Path
root=Path.cwd();run=Path(__file__).resolve().parent;read=lambda p:json.loads(p.read_text())
sha=lambda p:hashlib.sha256(p.read_bytes()).hexdigest()
frozen=read(run/'frozen-build.json');assert frozen['status']=='built-related-checks-passed'
for name,digest in frozen['sourceFiles'].items():assert hashlib.sha256((root/name).read_bytes().replace(b'\r\n',b'\n')).hexdigest()==digest,name
assert len(frozen['configurations'])==2
for configuration in frozen['configurations']:
    cfg=configuration['configuration'];build=root/('build/ninja-msvc-'+cfg)
    assert configuration['executedPassedCount']==11
    assert re.search(r'100% tests passed, 0 tests failed out of 11',(run/f'cpu-final-{cfg}.log').read_text())
    for name,digest in configuration['programs'].items():assert sha(build/(name+'.exe'))==digest
    assert {p.name:sha(p) for p in sorted((build/'shaders').glob('*.spv'))}==configuration['shaders']
for name in ['gpu-final-release','gpu-verified-debug']:
    report=read(run/name/'summary.json');assert report['status']=='passed' and len(report['captures'])==36 and len(report['comparisons'])==10
    assert all(row['maximumLinearError']==0 for row in report['comparisons'])
    assert report['ordinarySurfaceGuardExact'] and report['documentInlineMaximumLinearError']==0
native=read(run/'native-capture-summary.json');assert native['status']=='captured-inputs-unchanged' and len(native['cases'])==7
assert sum(row['manifest']['capturedFrames'] for row in native['cases'])==60
for name,digest in native['inputs'].items():assert sha(root/name)==digest,name
comparison=read(run/'native-comparison.json');assert comparison['comparisons'][0]['passed'] and comparison['hiddenEnvironmentExact'] and comparison['defaultNativeExact']
related=read(run/'related-gpu-summary.json');assert related['status']=='passed' and len(related['checks'])==9 and all(row['status']=='passed' for row in related['checks'])
boundary=read(run/'boundary-green.json');assert boundary['exitCode']!=0 and not boundary['failedToReject']
protected=read(root/'build/runs/20261031-053326-startup-context-d7dce6/preserved-user-files.json')
for name,digest in protected.items():assert sha(root/name)==digest,name
docs=['README.md','docs/plans/azure-engine-plan.md','docs/plans/2026-11-01-material-radiance-normalization.md','docs/runtime/material-radiance.md',
    'docs/runtime/character-material-profile.md','docs/runtime/unlit-texture-layers.md','docs/acceptance/nr/material-radiance-normalization.md','CHANGELOG.md']
subprocess.run([sys.executable,'tools/check_doc_style.py',*docs],check=True)
for name in docs:
    path=root/name
    for target in re.findall(r'\]\(([^)]+)\)',path.read_text()):
        if re.match(r'^[A-Za-z]+:',target) or target.startswith('#'):continue
        clean=target.split('#')[0].strip('<>');assert (path.parent/clean).exists(),(name,target)
changed=subprocess.check_output(['git','diff','--name-only','--relative'],text=True).splitlines();protected_names={name.replace('\\','/') for name in protected}
owned=[name for name in changed if name not in protected_names]
subprocess.run(['git','diff','--check','--',*owned],check=True)
report={'status':'passed','scope':'Material mechanism, source freeze, two actual 11-test selections, public GPU matrices, targeted regressions, native input checks, exact compatibility, docs and protected user changes.',
    'visualAcceptance':'failed-user-review','nativeFinalDisplayWithinTenPercent':False,'frozenSourceCommit':frozen['sourceCommit'],
    'publicCaptures':72,'nativeCaptures':60,'relatedChecks':len(related['checks']),'protectedUserFiles':len(protected),'ownedDiffPaths':owned}
(run/'verification.json').write_text(json.dumps(report,indent=2)+'\n');print(json.dumps(report))
