from pathlib import Path
import zipfile,hashlib,json
out=Path('build/p8-final-2026-10-13/arena-delivery-final');directory=out/'Moved Engine';archive=out/'packages/Azure-P8-final-engine.zip'
# Retain the first archive as failure evidence instead of silently replacing it.
archive.rename(out/'packages/Azure-P8-engine-filter-failed.zip')
def digest(p):return hashlib.sha256(p.read_bytes()).hexdigest()
files={p.relative_to(directory).as_posix():digest(p) for p in directory.rglob('*') if p.is_file()}
with zipfile.ZipFile(archive,'w',compression=zipfile.ZIP_DEFLATED,compresslevel=6) as z:
 for relative in sorted(files):z.write(directory/relative,directory.name+'/'+relative)
with zipfile.ZipFile(archive) as z:
 assert len(z.namelist())==len(files)
 for relative,expected in files.items():assert hashlib.sha256(z.read(directory.name+'/'+relative)).hexdigest()==expected
 manifest=json.loads(z.read(directory.name+'/install_manifest.json'))
 for row in manifest['files']:
  raw=z.read(directory.name+'/'+row['path']);assert len(raw)==row['size_bytes'];assert hashlib.sha256(raw).hexdigest()==row['sha256']
record=dict(kind='engine',path=str(archive.resolve()),sha256=digest(archive),bytes=archive.stat().st_size,files=len(files))
for path in (out/'packages/provenance.json',out/'summary.json'):
 data=json.loads(path.read_text(encoding='utf-8'));data['packages'][0]=record;data['archiveInstallManifestVerified']=True
 path.write_text(json.dumps(data,indent=2)+'\n',encoding='utf-8')
print(json.dumps(record))
