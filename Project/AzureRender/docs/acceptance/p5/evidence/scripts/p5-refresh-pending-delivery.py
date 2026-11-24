"""Deliver the actual pending-gate record while retaining tested products."""
import hashlib
import json
import shutil
from pathlib import Path
import subprocess
import zipfile

ROOT=Path.cwd().resolve();WORK=ROOT/'build/implementation-2026-10-10'
OUT=WORK/'p5-delivery';ENGINE=OUT/'Moved Engine'
def digest(path):
    with path.open('rb') as stream:return hashlib.file_digest(stream,'sha256').hexdigest()
def command(name,args):
    result=subprocess.run(list(map(str,args)),cwd=OUT,capture_output=True,text=True,encoding='utf-8',errors='replace')
    (OUT/(name+'.stdout.log')).write_text(result.stdout,encoding='utf-8')
    (OUT/(name+'.stderr.log')).write_text(result.stderr,encoding='utf-8')
    assert result.returncode==0,(name,result.stdout[-2000:]+result.stderr[-2000:])

assert json.loads((WORK/'p5-long-run/summary.json').read_text(encoding='utf-8'))['actualSeconds']>=1800
tested={p.name:digest(p) for p in (ENGINE/'bin').glob('*.exe')}
command('final-docs-install',['cmake','--install',ROOT/'build/ninja-msvc-release','--config','Release','--prefix',ENGINE])
assert tested=={p.name:digest(p) for p in (ENGINE/'bin').glob('*.exe')}
for name in ('AzureRender','AzurePlayer'):
    command('final-docs-'+name,['powershell.exe','-NoProfile','-ExecutionPolicy','Bypass','-File',
        ROOT/'tools/verify_windows_runtime.ps1','-Executable',ENGINE/'bin'/(name+'.exe')])
assert (ENGINE/'share/AzureRender/docs/acceptance/p5/evidence/manifest.json').is_file()
assert '正式性能门禁保持未通过' in (ENGINE/'share/AzureRender/docs/acceptance/p5/2026-10-10.md').read_text(encoding='utf-8')
manifest=ENGINE/'install_manifest.json'
command('final-docs-manifest-write',['cmake','-DINSTALL_DIR='+str(ENGINE),'-DOUTPUT_FILE='+str(manifest),'-P',ROOT/'tools/write_install_manifest.cmake'])
command('final-docs-manifest-verify',['cmake','-DINSTALL_DIR='+str(ENGINE),'-DMANIFEST_FILE='+str(manifest),'-P',ROOT/'tools/verify_install_manifest.cmake'])
files={p.relative_to(ENGINE).as_posix():digest(p) for p in ENGINE.rglob('*') if p.is_file() and 'captures' not in p.relative_to(ENGINE).parts and '.azure' not in p.relative_to(ENGINE).parts}
archive=OUT/'packages/Azure-P5-candidate-engine.zip'
retained=WORK/'p5-delivery-before-final-docs';retained.mkdir(exist_ok=False)
shutil.copy2(archive,retained/archive.name)
shutil.copy2(OUT/'packages/provenance.json',retained/'provenance.json')
with zipfile.ZipFile(archive,'w',compression=zipfile.ZIP_DEFLATED,compresslevel=6) as stream:
    for relative in sorted(files):stream.write(ENGINE/relative,ENGINE.name+'/'+relative)
with zipfile.ZipFile(archive) as stream:
    assert len(stream.namelist())==len(files)
    for relative,expected in files.items():assert hashlib.sha256(stream.read(ENGINE.name+'/'+relative)).hexdigest()==expected
summary_path=OUT/'summary.json';summary=json.loads(summary_path.read_text(encoding='utf-8'))
row=next(r for r in summary['packages'] if r['kind']=='engine')
row.update(sha256=digest(archive),bytes=archive.stat().st_size,files=len(files))
summary['finalDocumentation']={'passed':True,'productsUnchanged':tested,'installManifestVerified':True,'archiveFilesVerified':len(files)}
summary_path.write_text(json.dumps(summary,indent=2)+'\n',encoding='utf-8')
proof_path=OUT/'packages/provenance.json';proof=json.loads(proof_path.read_text(encoding='utf-8'))
proof['source']=json.loads((ROOT/'build/ninja-msvc-release/build-provenance.json').read_text(encoding='utf-8'))['source']
proof['packages']=summary['packages'];proof['finalDocumentation']=summary['finalDocumentation']
proof['status']='ActivePerformanceAndPhysicalInputPending'
proof['phaseComplete']=False
proof['remainingGates']=['working-set performance','physical input','subjective audio']
proof_path.write_text(json.dumps(proof,indent=2)+'\n',encoding='utf-8')
print(json.dumps(dict(passed=True,engineArchive=row,productsUnchanged=True)),flush=True)
