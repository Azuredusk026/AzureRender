import hashlib,json,re,subprocess,sys
from pathlib import Path
root=Path.cwd();run=Path(__file__).resolve().parent;sha=lambda p:hashlib.sha256(p.read_bytes()).hexdigest();read=lambda p:json.loads(p.read_text());candidate=read(run/'candidate.json');build=read(root/'build/runs/20261101-101614-view-fog-reuse-814a54/release-build.json');player=root/'build/ninja-msvc-release/AzurePlayer.exe';assert sha(player)==candidate['programSha256']==build['programs']['AzurePlayer'];assert {p.name:sha(p) for p in (player.parent/'shaders').glob('*.spv')}==candidate['shaders']==build['shaders']
for name,digest in build['sourceFiles'].items():assert hashlib.sha256((root/name).read_bytes().replace(b'\r\n',b'\n')).hexdigest()==digest,name
for name,digest in candidate['inputs'].items():assert sha(run/'project'/name)==digest,name
for name,digest in candidate['frozenSourceFiles'].items():assert sha(root/name)==digest,name
for spec_name,folder_name in [('diagnostic-inputs.json','diagnostic-project-v2'),('mask-inputs.json','mask-project')]:
 for name,digest in read(run/spec_name)['inputs'].items():assert sha(run/folder_name/name)==digest,name
total=0;instances=0
for report_name,folder_name,count in [('capture-summary.json','captures',4),('diagnostic-capture-summary.json','diagnostic-captures',3),('mask-capture-summary.json','mask-captures',1)]:
 report=read(run/report_name);assert report['status']=='captured-inputs-unchanged' and report['programSha256']==candidate['programSha256'] and len(report['cases'])==count
 for case in report['cases']:
  assert case['status']=='captured' and case['exitCode']==0;folder=run/folder_name/case['name'];images=case['images'];assert len(images)==(1 if folder_name=='diagnostic-captures' else 64)
  for name,digest in images.items():assert sha(folder/'frames'/name)==digest,name
  logs=[json.loads(line) for line in (folder/'captures/azureplayer.log.jsonl').read_text().splitlines()];assert not any(r['level']=='error' or r['subsystem']=='validation' and r['level']=='warning' for r in logs);assert any(r['message']=='Allocator after unload: buffers=0 images=0' for r in logs);assert not re.search(r'VUID-|Validation (?:Error|Warning)|SYNC-HAZARD|\[error\]',(folder/'player.log').read_text(errors='replace'),re.I);total+=len(images);instances+=1
assert total==323 and instances==8
whole=read(run/'comparison.json');assert len(whole['cases'])==2 and all(not c['allInteriorCriteriaPassed'] and len(c['rows'])==64 for c in whole['cases'])
materials=read(run/'material-comparison.json');assert len(materials['cases'])==6 and len(materials['diagnostics'])==3
for case in materials['cases']:
 rows={r['index']:r for r in case['materials']};assert all(rows[i]['coverage']=='measured' and not rows[i]['passed'] and rows[i]['medianLuminanceRelativeError']>1.1 for i in [2,5]);assert rows[0]['passed'] and rows[8]['passed']
failed=read(run/'failed-relative-diagnostic-capture-summary.json');assert failed['status']=='failed' and failed['cases'][0]['exitCode']!=0;assert 'Project path must be relative' in (run/'failed-relative-diagnostic-captures/fullbody-no-fog/player.log').read_text()
protected=read(root/'build/runs/20261031-053326-startup-context-d7dce6/preserved-user-files.json')
for name,digest in protected.items():assert sha(root/name)==digest,name
docs=['README.md','CHANGELOG.md','docs/plans/azure-engine-plan.md','docs/plans/2026-11-01-full-body-native-comparison.md','docs/acceptance/nr/full-body-native-comparison.md'];subprocess.run([sys.executable,'tools/check_doc_style.py',*docs],check=True)
for name in docs:
 p=root/name
 for target in re.findall(r'\]\(([^)]+)\)',p.read_text()):
  if re.match(r'^[A-Za-z]+:',target) or target.startswith('#'):continue
  assert (p.parent/target.split('#')[0].strip('<>')).exists(),(name,target)
archive=root/'docs/acceptance/nr/evidence/2026-11-01/full-body-native-comparison';m=read(archive/'manifest.json')
for r in m['files']:assert sha(archive/r['path'])==r['sha256'],r['path']
for r in m['reusedEvidence']:assert sha(root/r['path'])==r['sha256'],r['path']
result=dict(status='passed',visualAcceptance='failed-user-review',capturedFrames=total,successfulPlayerInstances=instances,sourceFiles=len(build['sourceFiles']),comparedWholeSilhouettePairs=128,materialComparisonCases=6,materialDiagnostics=3,preservedUserFiles=4);(run/'verification.json').write_bytes((json.dumps(result,indent=2)+'\n').encode());print(json.dumps(result))
