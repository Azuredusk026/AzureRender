from pathlib import Path
import copy,hashlib,json,math
import numpy as np
from PIL import Image
root=Path.cwd();run=Path(__file__).resolve().parent;spec=json.loads((run/'illumination-candidates.json').read_text());projectRoot=root/'build/runs/20261031-133610-native-character-visibility-92ad44/project';originalRoot=root/'build/runs/20261031-105539-full-scene-character-composition-b37668/project';source=json.loads((originalRoot/'scene/full-character-visible.azurelevel').read_text());sha=lambda p:hashlib.sha256(p.read_bytes()).hexdigest()
def equal(actual,expected):
 if isinstance(expected,list):return len(actual)==len(expected) and all(equal(a,b) for a,b in zip(actual,expected))
 if isinstance(expected,bool):return actual is expected
 if isinstance(expected,(int,float)):return math.isclose(actual,expected,rel_tol=0,abs_tol=1e-6)
 return actual==expected
records=[]
for case in spec['cases']:
 name=case['case'];project=root/case['project'];level=projectRoot/'scene'/('reference-illumination-'+name+'.azurelevel');assert sha(project)==case['projectSha256'] and sha(level)==case['levelSha256'];candidate=json.loads(level.read_text());restored=copy.deepcopy(candidate);restored['renderSettings']['nprLighting']=copy.deepcopy(source['renderSettings']['nprLighting'])
 for node in restored['nodes']:
  if name=='near' and node['id']=='camera':node['components']=copy.deepcopy(next(n['components'] for n in source['nodes'] if n['id']=='camera'))
  if name.endswith('hidden') and node['id']=='hero':
   original=next(n for n in source['nodes'] if n['id']=='hero')
   node['components']['azure.renderable']['data']['visible']=original['components']['azure.renderable']['data']['visible']
 assert restored==source,name+' unrelated scene input changed'
 manifest=json.loads((run/('illumination-'+name)/'frames/capture_manifest.json').read_text());expected=source['renderSettings']['nprLighting'] if name=='control-hidden' else spec['appliedPreset'];assert all(equal(manifest['nprLighting'][k],v) for k,v in expected.items()),name
 runtime=json.loads((run/('illumination-'+name)/'runtime.json').read_text());assert runtime['scriptErrors']==[] and runtime['presentationErrors']==[]
 records.append(dict(case=name,projectSha256=sha(project),levelSha256=sha(level),presetFieldsVerified=len(expected),sceneIntegrity='passed',objectShadowPrepared=manifest['objectShadowPrepared'],objectShadowPages=manifest['objectShadowPages']))
files=[]
for p in sorted((originalRoot/'assets/restored-character-materials').glob('*')):
 if p.is_file():q=projectRoot/p.relative_to(originalRoot);assert sha(p)==sha(q);files.append(dict(path=str(p.relative_to(originalRoot)),bytes=p.stat().st_size,sha256=sha(p)))
p=originalRoot/'content-character/hero-restored-inputs-v7.glb';q=projectRoot/p.relative_to(originalRoot);assert sha(p)==sha(q);files.append(dict(path=str(p.relative_to(originalRoot)),bytes=p.stat().st_size,sha256=sha(p)))
batch=json.loads((run/'illumination-full-summary.json').read_text());assert batch['hiddenCompatibility']['strictPngGatePassed'];assert len(batch['hiddenCompatibility']['frames'])==12
report=dict(status='input-integrity-and-hidden-compatibility-passed',visualAcceptance='pending',cases=records,unchangedCharacterFiles=files,hiddenFramesStrictlyEqual=12,scope='Data-only NPR preset candidate. Formal native visual, temporal, Debug and performance gates remain pending.')
(run/'illumination-verification.json').write_text(json.dumps(report,indent=2)+'\n');print(report['status'],len(files),'unchanged character files')
