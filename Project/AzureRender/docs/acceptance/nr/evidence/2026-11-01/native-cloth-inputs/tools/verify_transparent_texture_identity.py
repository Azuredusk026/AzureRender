import hashlib,json
from pathlib import Path
import numpy as np
from PIL import Image
run=Path(__file__).resolve().parent;root=run.parents[2];source=root/'build/runs/20261017-000956-endfield-native-3caf73/laevat-static'
audit=json.loads((source/'source_audit.json').read_text());artifacts=json.loads((source/'source_texture_artifacts.json').read_text());binding=json.loads((run/'transparent-bindings.json').read_text());assert binding['status']=='exported-transparent-bindings'
material=next(m for m in audit['materials'] if m['object']['name']=='M_actor_laevat_cloth_03');draw=binding['draws'][0];rows=[]
for slot,texture_slot in [('_BaseMap',19),('_MetallicGlossMap',20),('_SpecRampMap',15)]:
 reference=next(t['reference']['resolved'] for t in material['textures'] if t['slot']==slot)
 artifact=next(a for a in artifacts if a['object']['source']==reference['source'] and a['object']['path_id']==reference['path_id']);png=source/artifact['png']['path'];texture=next(t for t in draw['textures'] if t['slot']==texture_slot);dds=run/'transparent-bindings'/texture['export']['path']
 a=np.asarray(Image.open(png).convert('RGBA'));b=np.asarray(Image.open(dds).convert('RGBA'));assert a.shape==b.shape
 delta=np.abs(a[::-1].astype(np.int16)-b.astype(np.int16));row=dict(authorSlot=slot,runtimeSlot=texture_slot,sourceName=reference['name'],authorIdentity=reference,pngSha256=hashlib.sha256(png.read_bytes()).hexdigest(),ddsSha256=hashlib.sha256(dds.read_bytes()).hexdigest(),flipYMaximumByteError=int(delta.max()),flipYDifferentValues=int(np.count_nonzero(delta)));rows.append(row)
 assert row['flipYMaximumByteError']==0,row
report=dict(status='qualified-transparent-author-textures',event=1383,material=material['object'],textures=rows,qualification='Exact decompressed texture equality after the established vertical storage conversion; author identity and actual sampling slots are both recorded.')
(run/'transparent-texture-identity.json').write_bytes((json.dumps(report,indent=2)+'\n').encode());print(json.dumps(rows,indent=2))
