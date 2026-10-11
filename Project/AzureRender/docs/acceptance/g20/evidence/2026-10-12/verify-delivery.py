from pathlib import Path
import hashlib,json,os,shutil,subprocess,sys
root=Path.cwd().resolve();out=root/'build/g20-dependency-evaluation-2026-10-12';build=root/'build/ninja-msvc-release'
sys.path.insert(0,str(root/'tools'))
import build_game
from run_playable_long_run import isolated_environment
def run(name,args,cwd=out,env=None):
    p=subprocess.run(list(map(str,args)),cwd=cwd,env=env,capture_output=True,encoding='utf-8',errors='replace',timeout=300)
    (out/(name+'.log')).write_bytes((p.stdout+p.stderr).encode());assert p.returncode==0,(name,p.stdout,p.stderr)
install=out/'install';moved=out/'install-moved';fixtures=out/'audio-fixtures';project=out/'audio-project';package=out/'audio-package';destination=out/'Moved audio game'
if '--resume' not in sys.argv:
    run('fixtures',[build/'AzureAudioCodecTests.exe',fixtures]);run('authored-project',[build/'AzureEditorAudioWorkflowTests.exe',fixtures,project])
    run('install',['cmake','--install',build,'--config','Release','--prefix',install]);install.rename(moved)
    run('install-manifest',['cmake','-DINSTALL_DIR='+moved.as_posix(),'-DOUTPUT_FILE='+str(out/'install-manifest.json'),'-P',root/'tools/write_install_manifest.cmake'])
    run('install-verify',['cmake','-DINSTALL_DIR='+moved.as_posix(),'-DMANIFEST_FILE='+str(out/'install-manifest.json'),'-P',root/'tools/verify_install_manifest.cmake'])
    for binary in ('AzureRender','AzurePlayer'):run('isolated-'+binary,['powershell.exe','-NoProfile','-ExecutionPolicy','Bypass','-File',root/'tools/verify_windows_runtime.ps1','-Executable',moved/'bin'/(binary+'.exe')])
    run('publish',[sys.executable,root/'tools/build_game.py','--project',project/'project.azureproject','--install',moved,'--output',package])
    package.rename(destination);project.rename(out/'Project unavailable');moved.rename(out/'Engine unavailable')
manifest=build_game.verify_package(destination);moved=out/'Engine unavailable'
assert not project.exists() and not install.exists() and not (out/'install-moved').exists()
inventory=json.loads((root/'docs/acceptance/p1/evidence/2026-10-05/driver-inventory.json').read_text(encoding='utf-8'));intel=next(x for x in inventory if x['DriverDesc']=='Intel(R) UHD Graphics')
assert hashlib.sha256(Path(intel['VulkanDriverName']).read_bytes()).hexdigest()==intel['manifestSha256']
rows=[]
for device in ('nvidia','intel'):
    env=isolated_environment();env.update(VK_INSTANCE_LAYERS='VK_LAYER_KHRONOS_validation',VK_LAYER_VALIDATE_SYNC='1',VK_LOADER_DEBUG='error,layer')
    if device=='intel':env.update(VK_DRIVER_FILES=intel['VulkanDriverName'],VK_ICD_FILENAMES=intel['VulkanDriverName'])
    report=out/(device+'-runtime.json');capture=out/(device+'-capture')
    run('moved-'+device,[destination/'bin/AzurePlayer.exe','--project',destination/'game/project.azureproject','--resource-root',destination/'share/AzureRender','--runtime-report',report,'--capture-dir',capture,'--capture-frames','2','--capture-fps','4'],cwd=destination,env=env)
    data=json.loads(report.read_text(encoding='utf-8'));log=(out/('moved-'+device+'.log')).read_text(encoding='utf-8')
    assert data['audioStarts']==4 and not data['presentationErrors'],data
    assert all(text not in log.lower() for text in ('vuid-','validation error','sync-hazard'))
    assert 'Allocator after unload: buffers=0 images=0' in log and 'VK_LAYER_KHRONOS_validation' in log
    capabilities=json.loads((capture/'gpu_capabilities.json').read_text(encoding='utf-8'))
    assert ('Intel' if device=='intel' else 'NVIDIA') in capabilities['device_name']
    rows.append({'device':device,'audioStarts':data['audioStarts'],'presentationErrors':data['presentationErrors'],'capabilities':capabilities})
    build_game.verify_package(destination)
licenses={}
for name in ('miniaudio','stb','libvorbis','libogg','libflac','mp3lame'):
    file=moved/'share/AzureRender/licenses'/(name+'-LICENSE.txt');assert file.is_file();licenses[name]=hashlib.sha256(file.read_bytes()).hexdigest()
assert (moved/'share/AzureRender/licenses/audio-codecs-source.json').read_bytes()==(root/'docs/licenses/audio-codecs-source.json').read_bytes()
summary={'passed':True,'fileCount':json.loads((out/'install-manifest.json').read_text(encoding='utf-8'))['file_count'],'packageFiles':len(manifest['files']),'devices':rows,'sourceUnavailable':True,'engineUnavailable':True,'licenseHashes':licenses,'binaries':{name:hashlib.sha256((build/name).read_bytes()).hexdigest() for name in ('AzureRender.exe','AzurePlayer.exe')}}
(out/'delivery-summary.json').write_text(json.dumps(summary,indent=2)+'\n',encoding='utf-8');print('Moved isolated installation and dual-device four-format authored audio package passed')
