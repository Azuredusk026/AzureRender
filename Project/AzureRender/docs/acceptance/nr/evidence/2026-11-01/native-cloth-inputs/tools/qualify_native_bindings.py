import hashlib,json,re
from pathlib import Path
run=Path(__file__).resolve().parent;original=json.loads((run/'native-bindings.json').read_text());extra=json.loads((run/'variant-targets.json').read_text());rows=[]
for event in [868,889,910,928,1383]:
 source=original if event==868 else extra;draw=next(v for v in source['draws'] if v['event']==event)
 folder=run/('native-bindings' if event==868 else 'variant-targets')
 asm=folder/('e%d/ps.asm'%event);text=asm.read_text();slot=15 if event==1383 else 12
 samples=[line.strip() for line in text.splitlines() if re.search(r'sample_l\(texture2d\).*\bt%d\.'%slot,line)]
 assert len(samples)==1,samples
 texture=next(v for v in draw['textures'] if v['slot']==slot);assert texture['resource']=='ResourceId::6177' and texture['width']==texture['height']==256
 rows.append(dict(event=event,specularRampSlot=slot,resource=texture['resource'],sampleInstruction=samples[0],shaderSha256=draw['shader']['sha256'],assemblySha256=hashlib.sha256(asm.read_bytes()).hexdigest()))
shared=run/'native-bindings/textures/ResourceId-6177.dds';assert hashlib.sha256(shared.read_bytes()).hexdigest()=='b23e301482461a8b4ef44ac875cb9883e11aa507273413e42171dd0b8c113e1a'
report=dict(status='qualified-shared-specular-ramp',capture=original['capture'],captureSha256=original['captureSha256'],sharedTexture=dict(path=shared.relative_to(run).as_posix(),sha256=hashlib.sha256(shared.read_bytes()).hexdigest()),draws=rows,qualification='Resource identity, texture dimension and actual sampling instruction jointly qualify the binding. Transparent draw 1383 uses slot 15; its slot 12 is a three-dimensional lighting resource.')
(run/'native-binding-qualified.json').write_bytes((json.dumps(report,indent=2)+'\n').encode());print(json.dumps(rows,indent=2))
