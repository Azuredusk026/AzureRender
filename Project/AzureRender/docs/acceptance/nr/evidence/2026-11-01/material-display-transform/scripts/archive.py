import gzip,hashlib,json,os,subprocess
from pathlib import Path
root=Path.cwd();run=Path(__file__).resolve().parent;dest=root/'docs/acceptance/nr/evidence/2026-11-01/material-display-transform'
assert json.loads((run/'remaining-summary.json').read_text())['status']=='passed'
assert json.loads((run/'native-final-capture-summary.json').read_text())['status']=='captured-inputs-unchanged'
for cfg in ['debug','release']:
 assert json.loads((run/('gpu-complete-'+cfg)/'summary.json').read_text())['status']=='passed'
 assert json.loads((run/('related-gpu-'+cfg+'-summary.json')).read_text())['status']=='passed'
dest.mkdir(parents=True,exist_ok=False)
sha=lambda raw:hashlib.sha256(raw).hexdigest()
plain={'.py','.cpp','.hpp','.inc','.frag','.vert','.comp','.glsl','.json','.txt','.md'}
def copy(path,name):
 target=dest/name;target.parent.mkdir(parents=True,exist_ok=True);raw=path.read_bytes()
 target.write_bytes(raw.replace(b'\r\n',b'\n') if path.suffix in plain else raw)
def zipped(path,name):
 target=dest/name;target.parent.mkdir(parents=True,exist_ok=True);target.write_bytes(gzip.compress(path.read_bytes(),mtime=0))
for path in sorted(run.iterdir()):
 if not path.is_file():continue
 if path.suffix=='.json':copy(path,path.name)
 elif path.suffix in {'.log','.jsonl'}:zipped(path,'logs/'+path.name+'.gz')
 elif path.suffix=='.py':copy(path,'scripts/'+path.name);zipped(path,'scripts-raw/'+path.name+'.gz')
 elif path.suffix=='.png':copy(path,path.name)
prefixes=('gpu-','backend-gpu','supersample-gpu','related-gpu','native-captures','native-final-captures','display-views','temporal-views','fixture-preflight','field-fixture','fog-field','fog-radiance')
for folder in sorted(run.iterdir()):
 if not folder.is_dir() or not folder.name.startswith(prefixes):continue
 for directory,dirs,files in os.walk(folder):
  dirs[:]=[d for d in dirs if d not in {'.azure','.git','__pycache__'}]
  directory=Path(directory);frames=sorted(n for n in files if n.startswith('frame_') and n.endswith('.png'))
  keep={frames[0],frames[-1]} if frames else set()
  for filename in sorted(files):
   path=directory/filename;name=path.relative_to(run).as_posix()
   if path.suffix in {'.log','.jsonl'}:zipped(path,name+'.gz')
   elif filename in {'summary.json','report.json','result.json','metrics.json','input-fingerprints.json'}:copy(path,name)
   elif path.suffix in {'.json','.gltf','.azureproject','.azurelevel','.azurematerial','.azmeta','.azlut','.azfog','.bin'}:zipped(path,name+'.gz')
   elif path.suffix=='.png' and (directory.name!='frames' or filename in keep):copy(path,name)
   elif path.suffix=='.rgba':zipped(path,name+'.gz')
inputs=json.loads((run/'native-inputs.json').read_text());project=Path(inputs['project'])
for case in inputs['cases']:
 for field in ['project','level']:
  path=Path(case[field]);assert path.resolve().is_relative_to(project.resolve());zipped(path,'inputs/'+path.relative_to(project).as_posix()+'.gz')
for name in ['restored-character-materials','normalized-character-materials','linear-display-materials']:
 for path in sorted((project/'assets'/name).iterdir()):
  if path.is_file():zipped(path,'inputs/assets/'+name+'/'+path.name+'.gz')
scopes=['src','shaders','tests','tools','schemas','CMakeLists.txt'];changed=set()
for command in [['git','diff','--name-only','--relative','--',*scopes],['git','ls-files','--others','--exclude-standard','--',*scopes]]:
 changed.update(subprocess.check_output(command,cwd=root,text=True).splitlines())
source_copies=[]
for name in sorted(changed):
 path=root/name;assert path.is_file() and path.resolve().is_relative_to(root.resolve())
 copy(path,'source/'+name);source_copies.append(dict(path=name,canonicalSha256=sha(path.read_bytes().replace(b'\r\n',b'\n')),rawSha256=sha(path.read_bytes())))
 if path.suffix=='.py':zipped(path,'source-raw/'+name+'.gz')
for name in ['test_material_radiance_gpu.py','test_eye_npr_gpu.py','test_projected_hair_gpu.py','test_unlit_texture_layers_gpu.py','test_vertex_color_gpu.py','test_npr_illumination_gpu.py','test_view_fog_gpu.py','test_scene_fog_gpu.py','test_view_fog_msaa_gpu.py','test_reflection_history_gpu.py','create_visibility_fixture.py','convert_material_semantics.py']:
 path=root/'tools'/name
 if not (dest/'source/tools'/name).exists():copy(path,'source/tools/'+name);zipped(path,'source-raw/tools/'+name+'.gz')
files=[dict(path=p.relative_to(dest).as_posix(),bytes=p.stat().st_size,sha256=sha(p.read_bytes())) for p in sorted(dest.rglob('*')) if p.is_file()]
manifest=dict(schemaVersion=1,visualAcceptance='failed-user-review',baseCommit=subprocess.check_output(['git','rev-parse','HEAD'],text=True,cwd=root).strip(),
 canonicalText='UTF-8 with LF; captured logs and input payloads retain exact bytes in gzip streams.',sourceChanges=source_copies,files=files)
(dest/'manifest.json').write_bytes((json.dumps(manifest,indent=2)+'\n').encode())
print(json.dumps(dict(files=len(files),bytes=sum(row['bytes'] for row in files))))
