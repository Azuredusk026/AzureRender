from pathlib import Path
import json,numpy as np,math,hashlib
from PIL import Image
root=Path.cwd();r=Path(__file__).resolve().parent;a=np.asarray(Image.open(r/'control-beauty/frames/frame_000000.png').convert('RGB'));b=np.asarray(Image.open(r/'candidate-beauty/frames/frame_000000.png').convert('RGB'));albedo=np.asarray(Image.open(r/'control-albedo/frames/frame_000000.png').convert('RGB'));mask=(albedo[:,:,2]>albedo[:,:,1]*1.2)&(albedo[:,:,0]>albedo[:,:,1]*1.2)&(albedo[:,:,2]>90)&(albedo[:,:,1]>30);rows=[]
for name,box in [('image-left-iris',[625,755,697,829]),('image-right-iris',[838,704,912,781])]:
 x0,y0,x1,y1=box;m=mask[y0:y1,x0:x1];x=a[y0:y1,x0:x1][m];y=b[y0:y1,x0:x1][m];assert m.any();d=np.abs(x.astype('i2')-y.astype('i2'));rows.append(dict(region=name,rectangle=box,maskPixels=int(m.sum()),controlMeanRgb=x.mean(0).tolist(),candidateMeanRgb=y.mean(0).tolist(),changedPixels=int(np.any(d>0,axis=1).sum()),meanByteDifference=float(d.mean()),maximumByteDifference=int(d.max())))
actuals=[];spec=json.loads((r/'camera-candidate.json').read_text());preset=json.loads((root/'build/runs/20261031-155126-temporal-reference-quality-a9c045/illumination-candidates.json').read_text())['appliedPreset'];batch=json.loads((r/'summary.json').read_text());assert batch['status']=='captured-visual-review-pending' and len(batch['cases'])==3
for name in ['control-beauty','candidate-beauty','control-albedo']:
 p=r/name/'frames/capture_manifest.json';d=json.loads(p.read_text());assert all(abs(a-b)<5e-5 for a,b in zip(d['qaCameraPosition'],spec['cameraPosition']));assert abs(d['cameraLens']['verticalFovRadians']-math.radians(spec['fieldOfView']))<1e-6;assert d['antiAliasing']==0;assert d['viewFogMatch']=='camera-mismatch';assert d['objectShadowPrepared'] and d['objectShadowPages']==1
 if name=='candidate-beauty':
  for key,value in preset.items():
   actual=d['nprLighting'][key]
   if type(value) is bool:assert actual is value
   else:assert np.allclose(actual,value,rtol=0,atol=1e-6),key
 actuals.append(dict(case=name,qaCameraPosition=d['qaCameraPosition'],qaCameraTarget=d['qaCameraTarget'],cameraLens=d['cameraLens'],viewFogMatch=d['viewFogMatch'],objectShadowPrepared=d['objectShadowPrepared'],objectShadowPages=d['objectShadowPages'],pngSha256=hashlib.sha256((r/name/'frames/frame_000000.png').read_bytes()).hexdigest()))
assert actuals[0]['cameraLens']==actuals[1]['cameraLens']==actuals[2]['cameraLens']
report=dict(status='captured-camera-and-iris-regions-verified',visualAcceptance='pending',scope='Color-mask diagnostics inside manually frozen image rectangles. The mask selects violet iris pixels from the albedo view, includes some boundary pixels and is not a formal native visibility mask.',irisRegions=rows,captures=actuals);(r/'visual-verification.json').write_text(json.dumps(report,indent=2)+'\n');print(report['status'],[x['maskPixels'] for x in rows])
identities=json.loads((r/'input-verification.json').read_text())['sourceAssetFiles'];sha=lambda p:hashlib.sha256(p.read_bytes()).hexdigest()
for item in identities:assert sha(r/'project'/item['path'])==item['sha256'],item['path']
protected=json.loads((root/'build/runs/20261031-053326-startup-context-d7dce6/preserved-user-files.json').read_text());assert all(sha(root/name)==expected for name,expected in protected.items());(r/'postcapture-input-verification.json').write_text(json.dumps(dict(status='passed',unchangedAssetFiles=len(identities),protectedUserFiles=4),indent=2)+'\n');print('Post-capture source identities unchanged:',len(identities))
