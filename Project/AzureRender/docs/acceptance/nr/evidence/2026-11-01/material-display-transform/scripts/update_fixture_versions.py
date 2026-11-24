import hashlib,json
from pathlib import Path
root=Path.cwd();run=Path(__file__).resolve().parent;rows=[]
names=['test_character_screen_rim_gpu.py','test_eye_npr_gpu.py','test_material_radiance_gpu.py','test_npr_illumination_gpu.py','test_object_shadow_gpu.py',
    'test_scene_fog_gpu.py','test_unlit_texture_layers_gpu.py','test_vertex_color_gpu.py','test_view_fog_gpu.py','test_capture_camera_gpu.py']
for name in names:
    path=root/'tools'/name;raw=path.read_bytes();text=raw.decode('utf-8');old,new=("qaStateFingerprintVersion']==3","qaStateFingerprintVersion']==4") if name=='test_capture_camera_gpu.py' else ("materialSurfaceProfileLayoutVersion']==18","materialSurfaceProfileLayoutVersion']==19")
    assert old in text and new not in text,name
    replacement=text.replace(old,new).encode('utf-8')
    with path.open('r+b') as stream:stream.write(replacement);stream.truncate()
    rows.append({'file':path.relative_to(root).as_posix(),'sourceSha256':hashlib.sha256(raw).hexdigest(),'resultSha256':hashlib.sha256(replacement).hexdigest(),'change':'Version expectation only; numeric and visual budgets unchanged.'})
(run/'fixture-version-updates.json').write_text(json.dumps(rows,indent=2)+'\n');print('Updated',len(rows),'version expectations')
