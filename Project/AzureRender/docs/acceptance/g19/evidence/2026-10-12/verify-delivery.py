from pathlib import Path
import hashlib,json,subprocess
root=Path.cwd().resolve();out=root/'build/g19-dependency-evaluation-2026-10-12';install=out/'install-moved'
def run(name,args):
    p=subprocess.run(list(map(str,args)),capture_output=True,text=True,encoding='utf-8',errors='replace')
    (out/(name+'.log')).write_bytes((p.stdout+p.stderr).encode());assert p.returncode==0,(name,p.stdout,p.stderr)
manifest=out/'install-final-manifest.json'
run('install-final-manifest',['cmake','-DINSTALL_DIR='+install.as_posix(),'-DOUTPUT_FILE='+manifest.as_posix(),'-P',root/'tools/write_install_manifest.cmake'])
inventory=json.loads(manifest.read_text(encoding='utf-8'));assert inventory['file_count']>8000
run('install-final-verify',['cmake','-DINSTALL_DIR='+install.as_posix(),'-DMANIFEST_FILE='+manifest.as_posix(),'-P',root/'tools/verify_install_manifest.cmake'])
for binary in ('AzureRender','AzurePlayer'):
    run('isolated-final-'+binary,['powershell.exe','-NoProfile','-ExecutionPolicy','Bypass','-File',root/'tools/verify_windows_runtime.ps1','-Executable',install/'bin'/(binary+'.exe')])
licenses={}
for package in ('assimp','tiff','libwebp','libjpeg-turbo','liblzma'):
    file=install/'share/AzureRender/licenses'/(package+'-LICENSE.txt');assert file.is_file() and file.stat().st_size>100
    licenses[file.name]=hashlib.sha256(file.read_bytes()).hexdigest()
assert list((install/'bin').glob('assimp*.dll'))
for pattern in ('tiff*.dll','libwebp*.dll','jpeg*.dll','liblzma*.dll'):assert list((install/'bin').glob(pattern)),pattern
file=install/'share/AzureRender/licenses/image-codecs-source.json';assert file.read_bytes()==(root/'docs/licenses/image-codecs-source.json').read_bytes()
(out/'delivery-summary.json').write_bytes((json.dumps({'passed':True,'fileCount':inventory['file_count'],'licenseHashes':licenses,'installedBinaries':{name:hashlib.sha256((install/'bin'/name).read_bytes()).hexdigest() for name in ('AzureRender.exe','AzurePlayer.exe')}},indent=2)+'\n').encode())
print('Moved install, both isolated hosts, complete inventory and image codec runtime libraries and licenses passed')
