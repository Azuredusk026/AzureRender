import hashlib,json
from pathlib import Path
import numpy as np
from PIL import Image
root=Path.cwd();run=Path(__file__).resolve().parent;read=lambda p:json.loads(p.read_text());sources=read(run/'authored-texture-sources.json');profiles=read(root/'build/runs/20261101-120125-character-environment-reception-fa70a7/clothing-source-inspection.json');row_by_name={r['name']:r for r in sources['materials']}
for p in profiles:assert row_by_name[p['name']]['source']==p['other']['nativeCharacterProfileSource']['material'],p['name']
ramps=[next(t for t in row['textures'] if t['slot']=='_SpecRampMap') for row in sources['materials']];assert len(ramps)==5 and len({r['sha256'] for r in ramps})==1
native=root/'build/runs/20261026-062121-cloth-specular-ramp-capture-0e33fc/t12.dds';authored=root/ramps[0]['pngPath'];a=np.asarray(Image.open(native).convert('RGBA'));b=np.asarray(Image.open(authored).convert('RGBA'));assert a.shape==b.shape;error=int(np.max(np.abs(a.astype(np.int16)-b.astype(np.int16))));report=dict(status='pixel-identical' if error==0 else 'pixel-differences',shape=list(a.shape),maximumByteError=error,authoredMaterialIdentitiesVerified=5,commonAsset=ramps[0]['source'],nativeCloth4DdsSha256=hashlib.sha256(native.read_bytes()).hexdigest(),authoredPngSha256=ramps[0]['sha256'],qualification='Captured cloth four input and shared authored source are verified; captured bindings for other draws remain independently required.')
report['orientationControls']=[dict(transform=name,maximumByteError=int(np.abs(a.astype(np.int16)-v.astype(np.int16)).max()),meanByteError=float(np.abs(a.astype(np.int16)-v.astype(np.int16)).mean())) for name,v in [('identity',b),('flip-y',b[::-1]),('flip-x',b[:,::-1]),('flip-xy',b[::-1,::-1])]]
(run/'shared-specular-ramp-verification.json').write_bytes((json.dumps(report,indent=2)+'\n').encode());print(json.dumps(report))
