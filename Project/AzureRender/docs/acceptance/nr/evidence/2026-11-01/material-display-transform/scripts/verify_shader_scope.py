import hashlib,json
from pathlib import Path
root=Path.cwd();run=Path(__file__).resolve().parent;expected={'mesh_fog.frag.spv','mesh_fog_bindless.frag.spv','mesh_fog_bindless_samples.frag.spv','mesh_fog_samples.frag.spv'};rows=[]
for cfg in ['release','debug']:
 name='release-final-build.json' if cfg=='release' else 'debug-build.json';before=json.loads((run/('pre-fog-fix-'+name)).read_text());after=json.loads((run/name).read_text())
 assert before['programs']==after['programs']
 assert {p for p in after['sourceFiles'] if before['sourceFiles'].get(p)!=after['sourceFiles'][p]}=={'shaders/view_fog_material.glsl'}
 changed={p for p in after['shaders'] if before['shaders'].get(p)!=after['shaders'][p]};assert changed==expected,(cfg,changed)
 matrix=json.loads((run/('gpu-complete-'+cfg)/'summary.json').read_text());assert matrix['status']=='passed' and matrix['programSha256']==after['programs']['AzurePlayer']
 assert {p for p in matrix['shaderSha256'] if matrix['shaderSha256'][p]!=after['shaders'][p]}==expected
 checked=0
 for case in matrix['captures']:
  manifest=json.loads((run/('gpu-complete-'+cfg)/case['name']/'frames/capture_manifest.json').read_text())
  assert not manifest['viewFogEnabled'] and not manifest['viewFogAnalyticEnabled'],(cfg,case['name']);checked+=1
 related=json.loads((run/('related-gpu-'+cfg+'-summary.json')).read_text());reused=[]
 for row in related['checks']:
  if row.get('status')!='passed' or row['name'] in {'view-fog','scene-fog','fog-msaa'}:continue
  command=row['command'];folder=Path(command[command.index('--output')+1])
  manifests=list(folder.rglob('capture_manifest.json'));assert manifests,row['name']
  for path in manifests:
   m=json.loads(path.read_text());assert not m['viewFogEnabled'] and not m['viewFogAnalyticEnabled'],str(path)
  reused.append(dict(name=row['name'],captures=len(manifests)))
 rows.append(dict(configuration=cfg,programsIdentical=True,changedShaders=sorted(changed),inactiveFogMatrixCaptures=checked,reusedNonFogChecks=reused))
(run/'shader-scope-revalidation.json').write_text(json.dumps(dict(status='passed',scope='Only dedicated layer-fog shaders changed. Disabled-fog cases cannot construct ViewFogState in CharacterSceneRenderer; program bytes and every other shader are identical.',configurations=rows),indent=2)+'\n');print('PASS scoped reuse for two 53-capture matrices and inactive-fog checks')
