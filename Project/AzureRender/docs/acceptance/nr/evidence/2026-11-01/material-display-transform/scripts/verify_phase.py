import hashlib,json,re,subprocess,sys,gzip
from pathlib import Path
root=Path.cwd();run=Path(__file__).resolve().parent;read=lambda path:json.loads(path.read_text());sha=lambda path:hashlib.sha256(path.read_bytes()).hexdigest()
remaining=read(run/'remaining-summary.json');assert remaining['status']=='passed' and len(remaining['steps'])==8
assert read(run/'shader-scope-revalidation.json')['status']=='passed'
release=read(run/'release-final-build.json');debug=read(run/'debug-build.json');assert release['sourceFiles']==debug['sourceFiles']
for name,digest in release['sourceFiles'].items():assert hashlib.sha256((root/name).read_bytes().replace(b'\r\n',b'\n')).hexdigest()==digest,name
for cfg,build in [('release',release),('debug',debug)]:
 assert build['status']=='built-contract-tests-passed' and len(build['tests'])==12
 log=run/('cpu-release-final.log' if cfg=='release' else 'cpu-debug.log');assert re.search(r'100% tests passed, 0 tests failed out of 12',log.read_text())
 directory=root/('build/ninja-msvc-'+cfg)
 for name,digest in build['programs'].items():assert sha(directory/(name+'.exe'))==digest
 assert {p.name:sha(p) for p in sorted((directory/'shaders').glob('*.spv'))}==build['shaders']
 matrix=read(run/('gpu-complete-'+cfg)/'summary.json');assert matrix['status']=='passed' and len(matrix['captures'])==53 and len(matrix['comparisons'])==16
 assert matrix['programSha256']==build['programs']['AzurePlayer'] and all(row['maximumLinearError']<=.008 for row in matrix['comparisons'])
 assert matrix['independentEyeOracleError']<=.008 and sum(row['frames'] for row in matrix['captures'])==146
 related=read(run/('related-gpu-'+cfg+'-summary.json'));assert related['status']=='passed' and len(related['checks'])==(11 if cfg=='release' else 7)
 assert related['programSha256']==build['programs']['AzurePlayer'] and all(row['status']=='passed' for row in related['checks'])
 for row in related['checks']:assert row['scriptSha256']==sha(Path(row['command'][3])),row['name']
 backend=read(run/('backend-gpu-'+cfg+'-summary.json'));assert len(backend)==3 and all(row['status']=='passed' and row['programSha256']==build['programs']['AzurePlayer'] for row in backend)
 supersample=read(run/('supersample-gpu-'+cfg+'-summary.json'));assert supersample['status']=='passed' and supersample['programSha256']==build['programs']['AzurePlayer']
 views=read(run/('view-gpu-'+cfg+'-final-summary.json'));assert views['status']=='passed' and len(views['checks'])==4
 for row in views['checks']:assert row['programSha256']==sha(Path(row['command'][0])),row['name']
native=read(run/'native-final-capture-summary.json');assert native['status']=='captured-inputs-unchanged' and len(native['cases'])==6
assert native['programSha256']==release['programs']['AzurePlayer'] and sum(row['manifest']['capturedFrames'] for row in native['cases'])==59
for name,digest in native['inputs'].items():assert sha(root/name)==digest,name
comparison=read(run/'native-final-comparison.json');assert comparison['comparisons'][0]['passed'] and comparison['hiddenEnvironmentExact'] and comparison['defaultNativeExact']
assert comparison['characterInteriorMedianLuminancePassed'] and comparison['characterInteriorStaticSsimPassed']
protected=read(root/'build/runs/20261031-053326-startup-context-d7dce6/preserved-user-files.json')
for name,digest in protected.items():assert sha(root/name)==digest,name
green=[run/'gpu-complete-release',run/'gpu-complete-debug',run/'backend-gpu',run/'supersample-gpu',run/'native-final-captures']
for cfg in ['release','debug']:
 for row in read(run/('related-gpu-'+cfg+'-summary.json'))['checks']:
  command=row['command'];green.append(Path(command[command.index('--output')+1]))
checked_logs=0
for folder in green:
 for path in folder.rglob('*.log'):
  assert not re.search(r'VUID-|Validation (?:Error|Warning)|SYNC-HAZARD|\[error\]',path.read_text(errors='replace'),re.I),str(path)
  checked_logs+=1
 for path in folder.rglob('azureplayer.log.jsonl'):
  rows=[json.loads(line) for line in path.read_text().splitlines()]
  assert not any(row['level']=='error' or row['subsystem']=='validation' and row['level']=='warning' for row in rows),str(path)
docs=['README.md','CHANGELOG.md','docs/plans/azure-engine-plan.md','docs/plans/2026-11-01-material-display-transform.md',
 'docs/acceptance/nr/material-display-transform.md','docs/runtime/material-display-transform.md','docs/runtime/material-radiance.md','docs/runtime/character-material-profile.md',
 'docs/runtime/unlit-texture-layers.md','docs/runtime/vertex-color-inputs.md','docs/runtime/multisample-shading.md','docs/runtime/temporal-antialiasing.md','docs/runtime/temporal-frame-state.md']
subprocess.run([sys.executable,'tools/check_doc_style.py',*docs],check=True)
for name in docs:
 path=root/name
 for target in re.findall(r'\]\(([^)]+)\)',path.read_text()):
  if re.match(r'^[A-Za-z]+:',target) or target.startswith('#'):continue
  assert (path.parent/target.split('#')[0].strip('<>')).exists(),(name,target)
protected_names={name.replace('\\','/') for name in protected}
owned=[name for name in subprocess.check_output(['git','diff','--name-only','--relative'],text=True).splitlines() if name not in protected_names]
subprocess.run(['git','diff','--check','--',*owned],check=True)
report=dict(status='passed',visualAcceptance='failed-user-review',frozenSourceFiles=len(release['sourceFiles']),cpuChecksPerConfiguration=12,
 publicCapturesPerConfiguration=53,publicFramesPerConfiguration=146,nativeFrames=59,relatedChecks=18,validatedPlayerLogs=checked_logs,preservedUserFiles=len(protected))
(run/'verification.json').write_text(json.dumps(report,indent=2)+'\n');print(json.dumps(report))
