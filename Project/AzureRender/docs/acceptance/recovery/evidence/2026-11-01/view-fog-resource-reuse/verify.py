import hashlib,json,re,subprocess,sys
from pathlib import Path
root=Path.cwd();run=Path(__file__).resolve().parent;read=lambda p:json.loads(p.read_text());sha=lambda p:hashlib.sha256(p.read_bytes()).hexdigest();before=root/'build/runs/20261101-082926-joint-playable-display-c26c1b'
assert read(run/'red-cpu.json')['status']=='red-confirmed' and read(run/'red-gpu.json')['status']=='red-confirmed' and read(run/'red-temporal-gpu.json')['status']=='red-confirmed'
release=read(run/'release-build.json');debug=read(run/'debug-build.json');assert release['sourceFiles']==debug['sourceFiles']
for name,digest in release['sourceFiles'].items():assert hashlib.sha256((root/name).read_bytes().replace(b'\r\n',b'\n')).hexdigest()==digest,name
protocol=read(run/'protocol-summary.json');assert protocol['status']=='passed' and len(protocol['checks'])==2
previous=read(before/'build-reuse.json')
for cfg,build in [('release',release),('debug',debug)]:
 assert build['status']=='built-contract-tests-passed' and len(build['tests'])==10 and '100% tests passed, 0 tests failed out of 10' in (run/('cpu-'+cfg+'.log')).read_text()
 directory=root/('build/ninja-msvc-'+cfg)
 for name,digest in build['programs'].items():assert sha(directory/(name+'.exe'))==digest,name
 assert {p.name:sha(p) for p in (directory/'shaders').glob('*.spv')}==build['shaders']==previous['configurations'][cfg]['shaders']
 row=next(row for row in protocol['checks'] if row['configuration']==cfg);assert sha(directory/'AzureValidationProbe.exe')==row['probeSha256']
 assert '100% tests passed, 0 tests failed out of 1' in (run/('protocol-test-'+cfg+'.log')).read_text()
tools_record=read(run/'tool-deadline-fix.json');assert sha(root/'tools/test_renderer_residency.py')==tools_record['scriptSha256'] and sha(root/'tools/test_validation_protocol.py')==tools_record['protocolHelperSha256'];assert tools_record['transitionDeadlineSeconds']==45 and tools_record['unchangedOuterProcessDeadlineSeconds']==300
public=read(run/'public-gpu-summary.json');assert public['status']=='passed' and len(public['checks'])==2
folders=[]
for row in public['checks']:
 cfg=row['configuration'];build=release if cfg=='release' else debug;assert row['status']=='passed' and row['transitions']==32 and row['workflows']==4 and row['playerSha256']==build['programs']['AzurePlayer']
 assert row['scriptSha256']==tools_record['scriptSha256'] and row['protocolHelperSha256']==tools_record['protocolHelperSha256']
 out=next((run/'public-gpu-green'/cfg).glob('run-*'));cases=read(out/'summary.json')['cases'];assert len(cases)==4 and all(c['unchangedBindingReused'] and c['released'] for c in cases)
 for folder in out.iterdir():
  if folder.is_dir():
   observations=read(folder/'observations.json');assert observations[0]['entries']==1 and all(row['entries']<=2 for row in observations);folders.append(folder)
private=read(run/'private-gpu-summary.json');assert private['status']=='passed' and len(private['checks'])==2
for row in private['checks']:
 cfg=row['configuration'];assert row['frames']==540 and row['baselineIdenticalImages']==540 and not row['differentImages'] and row['rendererCpuPreparations']==1
 folder=run/('route-'+cfg);assert read(folder/'behavior-verification.json')['status']=='passed';folders.append(folder)
 baseline=read(before/('route-'+cfg)/'frame-fingerprints.json');assert len(baseline)==540
 for name,digest in baseline.items():assert sha(folder/'frames'/name)==digest,name
assert read(run/'fixed-world-summary.json')['status']=='passed' and read(run/'fixed-world-summary.json')['pixelExactFrames']==12;folders.append(run/'fixed-world')
assert len(folders)==11
for folder in folders:
 if (folder/'player.log').exists():texts=[(folder/'player.log').read_text(errors='replace')]
 else:texts=[(folder/name).read_text(errors='replace') for name in ['stdout.log','stderr.log']]
 assert not any(re.search(r'VUID-|Validation (?:Error|Warning)|SYNC-HAZARD|\[error\]',text,re.I) for text in texts),str(folder)
 logs=[json.loads(line) for line in (folder/'captures/azureplayer.log.jsonl').read_text().splitlines()]
 assert not any(row['level']=='error' or row['subsystem']=='validation' and row['level']=='warning' for row in logs),str(folder)
 assert any(row['message']=='Allocator after unload: buffers=0 images=0' for row in logs),str(folder)
 if (folder/'frames/capture_manifest.json').exists():
  manifest=read(folder/'frames/capture_manifest.json');assert manifest['materialSurfaceProfileLayoutVersion']==19 and len(manifest['materialDisplayTransforms'])==11
candidate=read(run/'candidate.json')
for name,digest in candidate['inputs'].items():assert sha(run/'project'/name)==digest,name
for name,digest in candidate['frozenSourceFiles'].items():assert sha(root/name)==digest,name
protected=read(root/'build/runs/20261031-053326-startup-context-d7dce6/preserved-user-files.json')
for name,digest in protected.items():assert sha(root/name)==digest,name
docs=['README.md','CHANGELOG.md','docs/plans/azure-engine-plan.md','docs/plans/2026-11-01-view-fog-resource-reuse.md','docs/runtime/render-residency.md','docs/acceptance/recovery/view-fog-resource-reuse.md']
subprocess.run([sys.executable,'tools/check_doc_style.py',*docs],check=True)
for name in docs:
 path=root/name
 for target in re.findall(r'\]\(([^)]+)\)',path.read_text()):
  if re.match(r'^[A-Za-z]+:',target) or target.startswith('#'):continue
  assert (path.parent/target.split('#')[0].strip('<>')).exists(),(name,target)
record=dict(status='passed',visualAcceptance='failed-user-review',frozenSourceFiles=len(release['sourceFiles']),cpuChecksPerConfiguration=11,publicWorkflowsPerConfiguration=4,publicTransitionsPerConfiguration=32,routeFramesPerConfiguration=540,fixedWorldExactFrames=12,capturedFrames=1092,validatedSuccessfulPlayerInstances=11,rendererPreparationsPerFullRoute=1,preservedUserFiles=len(protected))
(run/'verification.json').write_bytes((json.dumps(record,indent=2)+'\n').encode());print(json.dumps(record))
