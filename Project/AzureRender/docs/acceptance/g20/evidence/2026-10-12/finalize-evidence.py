from pathlib import Path
import gzip,hashlib,json,shutil,subprocess
root=Path.cwd();out=root/'build/g20-dependency-evaluation-2026-10-12';dest=root/'docs/acceptance/g20/evidence/2026-10-12'
assert 'Documentation built in' in (out/'site-build.log').read_text(encoding='utf-8')
for name in ('site-build.log','archive-stage.log'):(dest/(name+'.gz')).write_bytes(gzip.compress((out/name).read_bytes(),mtime=0))
for name in ('nvidia-capture','intel-capture'):shutil.copytree(out/name,dest/name,dirs_exist_ok=True)
shutil.copy2(Path(__file__),dest/Path(__file__).name)
p=dest/'manifest.json';manifest=json.loads(p.read_text(encoding='utf-8'));manifest['files']=[]
for file in sorted(dest.rglob('*')):
    if file.is_file() and file.name!='manifest.json':manifest['files'].append({'path':file.relative_to(dest).as_posix(),'sha256':hashlib.sha256(file.read_bytes()).hexdigest(),'bytes':file.stat().st_size})
p.write_text(json.dumps(manifest,indent=2)+'\n',encoding='utf-8');subprocess.run(['git','add','-f','--',str(dest)],check=True)
for entry in manifest['files']:
    blob=subprocess.check_output(['git','show',':Project/AzureRender/'+dest.relative_to(root).as_posix()+'/'+entry['path']]);assert hashlib.sha256(blob).hexdigest()==entry['sha256'],entry['path']
for file in out.glob('*.log'):
    archived=dest/(file.name+'.gz')
    if archived.exists():assert gzip.decompress(archived.read_bytes())==file.read_bytes(),file.name
print('Verified',len(manifest['files']),'raw evidence blobs, compressed source logs, site build and both package captures')
