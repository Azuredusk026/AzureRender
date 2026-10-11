from pathlib import Path
import gzip,hashlib,json,shutil,subprocess,sys
root=Path.cwd();out=root/'build/g21-dependency-evaluation-2026-10-12';dest=root/'docs/acceptance/g21/evidence/2026-10-12'
assert 'Documentation built in' in (out/'site-build.log').read_text(encoding='utf-8')
assert 'passed for 9 files' in (out/'document-style.log').read_text(encoding='utf-8')
delivery=json.loads((out/'delivery-summary.json').read_text(encoding='utf-8'));assert delivery['passed'] and delivery['installedDocumentsMatch']
sys.path.insert(0,str(root/'tools'));from build_provenance import verify
for config in ('debug','release'):verify(root,json.loads((root/('build/ninja-msvc-'+config)/'build-provenance.json').read_text(encoding='utf-8')))
for file in out.glob('*.log'):(dest/(file.name+'.gz')).write_bytes(gzip.compress(file.read_bytes(),mtime=0))
for file in out.glob('*.py'):shutil.copy2(file,dest/file.name)
shutil.copy2(out/'delivery-summary.json',dest/'delivery-summary.json')
(dest/'install-manifest.json.gz').write_bytes(gzip.compress((out/'install-manifest.json').read_bytes(),mtime=0))
p=dest/'manifest.json';manifest=json.loads(p.read_text(encoding='utf-8'));manifest['installedFileCount']=delivery['fileCount'];manifest['installedDocumentsMatch']=True;manifest['files']=[]
for file in sorted(dest.rglob('*')):
    if file.is_file() and file.name!='manifest.json':manifest['files'].append({'path':file.relative_to(dest).as_posix(),'sha256':hashlib.sha256(file.read_bytes()).hexdigest(),'bytes':file.stat().st_size})
p.write_text(json.dumps(manifest,indent=2)+'\n',encoding='utf-8')
subprocess.run(['git','add','--','docs/runtime/asset-formats.md'],check=True)
subprocess.run(['git','add','-f','--',str(dest)],check=True)
for entry in manifest['files']:
    blob=subprocess.check_output(['git','show',':Project/AzureRender/'+dest.relative_to(root).as_posix()+'/'+entry['path']]);assert hashlib.sha256(blob).hexdigest()==entry['sha256'],entry['path']
for file in out.glob('*.log'):
    archived=dest/(file.name+'.gz');assert gzip.decompress(archived.read_bytes())==file.read_bytes(),file.name
print('Verified',len(manifest['files']),'raw evidence blobs, final source proofs, install and dual-device captures')
