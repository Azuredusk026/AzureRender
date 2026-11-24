"""Archive and verify machine-tested candidate packages with portable provenance."""
import hashlib
import json
from pathlib import Path
import sys
import zipfile

ROOT=Path.cwd().resolve();sys.path.insert(0,str(ROOT/'tools'))
from build_game import verify_package
from build_provenance import verify
release=ROOT/'build/ninja-msvc-release'
proof=json.loads((release/'build-provenance.json').read_text(encoding='utf-8'));verify(ROOT,proof)
delivery=max((ROOT/'build/p4/delivery').glob('run-*/summary.json'),key=lambda p:p.stat().st_mtime)
summary=json.loads(delivery.read_text(encoding='utf-8'));assert summary['passed']
OUT=ROOT/'build/p4/packages';OUT.mkdir(exist_ok=False)
rows=[]
for kind,directory in [('engine',Path(summary['engine']))]+[(r['workflow'],Path(r['path'])) for r in summary['packages']]:
    if kind!='engine':verify_package(directory)
    else:
        import subprocess
        subprocess.run(['cmake','-DINSTALL_DIR='+str(directory),'-DMANIFEST_FILE='+str(directory/'install_manifest.json'),'-P',str(ROOT/'tools/verify_install_manifest.cmake')],check=True)
    archive=OUT/('AzureEngine-P4-candidate-'+kind+'.zip')
    files={p.relative_to(directory).as_posix():hashlib.sha256(p.read_bytes()).hexdigest() for p in directory.rglob('*') if p.is_file()}
    with zipfile.ZipFile(archive,'w',compression=zipfile.ZIP_DEFLATED,compresslevel=6) as stream:
        for relative in sorted(files):stream.write(directory/relative,directory.name+'/'+relative)
    with zipfile.ZipFile(archive) as stream:
        assert len(stream.namelist())==len(files)
        for relative,expected in files.items():assert hashlib.sha256(stream.read(directory.name+'/'+relative)).hexdigest()==expected
    rows.append(dict(kind=kind,path=str(archive),sha256=hashlib.sha256(archive.read_bytes()).hexdigest(),bytes=archive.stat().st_size,files=len(files),manifestSha256=hashlib.sha256((directory/('install_manifest.json' if kind=='engine' else 'game_manifest.json')).read_bytes()).hexdigest()))
portable=dict(schemaVersion=1,status='MachineVerifiedPhysicalInputPending',configuration='Release',baseCommit=proof['commit'],
    source=proof['source'],products={Path(n).relative_to(release).as_posix():h for n,h in proof['products'].items()},packages=rows)
(OUT/'provenance.json').write_text(json.dumps(portable,indent=2),encoding='utf-8')
(ROOT/'build/p4/delivery-packages.json').write_text(json.dumps(dict(passed=True,packages=rows,provenance=str(OUT/'provenance.json')),indent=2),encoding='utf-8')
print(json.dumps(dict(passed=True,packages=rows)),flush=True)
