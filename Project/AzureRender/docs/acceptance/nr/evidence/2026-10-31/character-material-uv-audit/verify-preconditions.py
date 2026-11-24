from pathlib import Path
import json,sys,copy,hashlib
root=Path.cwd();r=Path(__file__).resolve().parent;sys.path.insert(0,str(root/'tools'));from retarget_animation import read_glb;from endfield_native.mesh_uv import restore_primary_uv
paths=[root/'build/runs/20261031-135416-character-portrait-recovery-0773ca/original-reference/project/native-character/outline-converted.glb',root/'build/runs/20261031-172112-hair-source-inputs-14d199/project/content-character/hero-restored-inputs-v8.glb'];loaded=[]
for p in paths:d,c=read_glb(p);loaded.append((d,next(b for k,b in c if k==0x004e4942)))
(reference,ref_binary),(document,binary)=loaded;before=copy.deepcopy(document);rows=[]
for slot in ['name:M_actor_laevat_body_01','name:M_actor_laevat_cloth_04','name:M_actor_laevat_brow_01']:
 try:restore_primary_uv(document,binary,reference,ref_binary,[slot]);raise AssertionError('Unverified source correspondence was accepted')
 except ValueError as error:rows.append(dict(slot=slot,rejected=True,diagnostic=str(error)))
 assert before==document
report=dict(status='actual-source-preconditions-enforced',sourceCommit='4188d66',toolSha256=hashlib.sha256((root/'tools/endfield_native/mesh_uv.py').read_bytes()).hexdigest(),cases=rows,sourceDocumentPreserved=True,scope='Topology or tangent-sign rejection is expected. Each material requires its own source correspondence; no UV changes were applied.');(r/'source-preconditions.json').write_text(json.dumps(report,indent=2)+'\n');print(json.dumps(report,indent=2))
