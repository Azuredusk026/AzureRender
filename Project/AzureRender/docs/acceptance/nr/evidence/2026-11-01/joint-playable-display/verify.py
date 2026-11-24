import hashlib,json,re,subprocess,sys
from pathlib import Path
root=Path.cwd();run=Path(__file__).resolve().parent;read=lambda p:json.loads(p.read_text());sha=lambda p:hashlib.sha256(p.read_bytes()).hexdigest()
subprocess.run([sys.executable,'-X','utf8',str(run/'verify_build_reuse.py')],check=True)
build=read(run/'build-reuse.json');spec=read(run/'candidate.json');project=root/spec['project'];assert read(run/'project-check/summary.json')['status']=='passed'
for name,digest in spec['inputs'].items():assert sha(project.parent/name)==digest,name
for name,digest in spec['frozenSourceFiles'].items():assert sha(root/name)==digest,name
standalone=read(run/'standalone-inputs.json');assert sha(Path(standalone['sourceOriginal']))==standalone['sourceOriginalSha256']
for name,digest in standalone['inputs'].items():assert sha(run/'standalone-project'/name)==digest,name
shadow_inputs=read(run/'object-shadow-inputs.json')
for name,digest in shadow_inputs['inputs'].items():assert sha(run/'standalone-project'/name)==digest,name
assert read(run/'review.json')['status']=='behavior-cross-configuration-passed'
for cfg in ['release','debug']:
 folder=run/('route-'+cfg);row=read(folder/'summary.json');assert row['status']=='behavior-passed-visual-review-pending'
 assert row['programSha256']==build['configurations'][cfg]['programs']['AzurePlayer']
 assert read(folder/'behavior-verification.json')['status']=='passed'
 for name,digest in read(folder/'frame-fingerprints.json').items():assert sha(folder/'frames'/name)==digest,name
entry=read(run/'native-entry/summary.json');assert entry['status']=='captured' and entry['pixelExact'] and entry['qaFlagsProvided'] is False
effects=read(run/'native-effects-summary.json');assert effects['status']=='measured' and len(effects['cases'])==8
character_effects=read(run/'native-character-effects.json');assert character_effects['sourceReportSha256']==sha(run/'native-effects-summary.json')
assert character_effects['characterResponseBudgetPassed'] and len(character_effects['characterComparisons'])==20
assert len(character_effects['environmentControlDiagnostics'])==8 and character_effects['linearResponseErrorBudget']==.008
assert all(e['nonzeroReferenceRegions'] for e in character_effects['effects'] if e['effect']!='shadow')
assert read(run/'standalone-capture-summary.json')['status']=='captured-inputs-unchanged'
assert all(c['interiorCriteriaPassed'] for c in read(run/'standalone-comparison.json')['cases'])
shadow=read(run/'object-shadow-summary.json');assert shadow['status']=='measured' and shadow['responseBudgetPassed'] and shadow['hasNonzeroSkinResponse'] and len(shadow['comparisons'])==4
diagnostics=read(run/'configuration-image-diagnostics.json');assert diagnostics['status']=='measured' and diagnostics['exactImageFiles']+diagnostics['differentImageFiles']==540 and len(diagnostics['rows'])==141
folders=[run/'route-release',run/'route-debug',run/'native-entry']+[p for group in ['native-effects','standalone-captures','object-shadow-captures'] for p in (run/group).iterdir() if p.is_dir()]
assert len(folders)==15;frames=0
for folder in folders:
 text=(folder/'player.log').read_text(errors='replace');assert not re.search(r'VUID-|Validation (?:Error|Warning)|SYNC-HAZARD|\[error\]',text,re.I),str(folder)
 rows=[json.loads(line) for line in (folder/'captures/azureplayer.log.jsonl').read_text().splitlines()]
 assert not any(r['level']=='error' or r['subsystem']=='validation' and r['level']=='warning' for r in rows),str(folder)
 assert any(r['message']=='Allocator after unload: buffers=0 images=0' for r in rows),str(folder)
 manifest=read(folder/'frames/capture_manifest.json');assert manifest['materialSurfaceProfileLayoutVersion']==19 and len(manifest['materialDisplayTransforms'])==11
 assert len(list((folder/'frames').glob('frame_*.png')))==manifest['capturedFrames'];frames+=manifest['capturedFrames']
 runtime=read(folder/'runtime.json');assert not runtime['scriptErrors'] and not runtime['presentationErrors']
assert frames==1095
protected=read(root/'build/runs/20261031-053326-startup-context-d7dce6/preserved-user-files.json')
for name,digest in protected.items():assert sha(root/name)==digest,name
docs=['README.md','CHANGELOG.md','docs/plans/azure-engine-plan.md','docs/plans/2026-11-01-joint-playable-display.md','docs/acceptance/nr/joint-playable-display.md']
subprocess.run([sys.executable,'tools/check_doc_style.py',*docs],check=True)
for name in docs:
 path=root/name
 for target in re.findall(r'\]\(([^)]+)\)',path.read_text()):
  if re.match(r'^[A-Za-z]+:',target) or target.startswith('#'):continue
  assert (path.parent/target.split('#')[0].strip('<>')).exists(),(name,target)
manifest=read(root/'docs/development/artifact-manifest.json');pin=next(p for p in manifest['preservedRuns'] if p['path'].endswith(run.name));assert (run/pin['pinMarker']).exists() and (run/pin['primaryProject']).exists()
record=dict(status='passed',visualAcceptance='failed-user-review',scope='Composition, input behavior, source reuse, native affected regions and lifecycle only.',frozenSourceFiles=988,validatedPlayerInstances=len(folders),capturedFrames=frames,framesPerInputRoute=540,characterEffectComparisons=20,environmentControlDiagnostics=8,objectShadowSkinComparisons=4,originalProjectInputFiles=len(spec['frozenSourceFiles']),preservedUserFiles=len(protected))
(run/'verification.json').write_bytes((json.dumps(record,indent=2)+'\n').encode());print(json.dumps(record))
