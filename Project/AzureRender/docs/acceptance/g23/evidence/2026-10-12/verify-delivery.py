from pathlib import Path
import hashlib,json,os,shutil,subprocess,sys
root=Path.cwd().resolve();out=root/'build/g23-compressed-audio-2026-10-12';build=root/'build/ninja-msvc-release'
sys.path.insert(0,str(root/'tools'));import build_game
from run_playable_long_run import isolated_environment

def run(name,args,cwd=out,env=None):
    result=subprocess.run(list(map(str,args)),cwd=cwd,env=env,capture_output=True,encoding='utf-8',errors='replace',timeout=300)
    (out/(name+'.log')).write_bytes((result.stdout+result.stderr).encode());assert result.returncode==0,(name,result.stdout,result.stderr)
install=out/'install';engine=out/'Engine unavailable';projects=out/'authoring';hidden=out/'Sources unavailable'
if '--resume' not in sys.argv and '--validate-only' not in sys.argv:
    run('authored-projects',[build/'AzureCompressedAudioFormatsTests.exe',root/'docs/acceptance/g23/evidence/2026-10-12/audio-fixtures',root/'assets_public/test_model.gltf',projects])
    run('install',['cmake','--install',build,'--config','Release','--prefix',install]);install.rename(engine)
    run('install-manifest',['cmake','-DINSTALL_DIR='+engine.as_posix(),'-DOUTPUT_FILE='+str(out/'install-manifest.json'),'-P',root/'tools/write_install_manifest.cmake'])
    run('install-verify',['cmake','-DINSTALL_DIR='+engine.as_posix(),'-DMANIFEST_FILE='+str(out/'install-manifest.json'),'-P',root/'tools/verify_install_manifest.cmake'])
    for binary in ('AzureRender','AzurePlayer'):run('isolated-'+binary,['powershell.exe','-NoProfile','-ExecutionPolicy','Bypass','-File',root/'tools/verify_windows_runtime.ps1','-Executable',engine/'bin'/(binary+'.exe')])
    for workflow in ('opus','m4a'):
        package=out/('package-'+workflow);run('publish-'+workflow,[sys.executable,root/'tools/build_game.py','--project',projects/workflow/'project.azureproject','--install',engine,'--output',package]);package.rename(out/('Moved '+workflow))
    projects.rename(hidden)
if '--prepare-only' in sys.argv:sys.exit(0)
assert not projects.exists() and not install.exists()
inventory=json.loads((root/'docs/acceptance/p1/evidence/2026-10-05/driver-inventory.json').read_text(encoding='utf-8'));intel=next(x for x in inventory if x['DriverDesc']=='Intel(R) UHD Graphics')
assert hashlib.sha256(Path(intel['VulkanDriverName']).read_bytes()).hexdigest()==intel['manifestSha256']
rows=[]
for device in ('nvidia','intel'):
    env=isolated_environment();env.update(VK_INSTANCE_LAYERS='VK_LAYER_KHRONOS_validation',VK_LAYER_VALIDATE_SYNC='1',VK_LOADER_DEBUG='error,layer')
    if device=='intel':env.update(VK_DRIVER_FILES=intel['VulkanDriverName'],VK_ICD_FILENAMES=intel['VulkanDriverName'])
    for workflow in ('opus','m4a'):
        name=device+'-'+workflow;package=out/('Moved '+workflow);manifest=build_game.verify_package(package)
        for library in ('opus','opusfile','mp3lame'):
            assert (package/'share/AzureRender/licenses'/(library+'-LICENSE.txt')).read_bytes()==(engine/'share/AzureRender/licenses'/(library+'-LICENSE.txt')).read_bytes()
        for notice in (root/'docs/licenses/compressed-audio').glob('*.txt'):
            assert (package/'share/AzureRender/licenses/compressed-audio'/notice.name).read_bytes()==notice.read_bytes()
        report=out/(name+'-runtime.json');capture=out/(name+'-capture')
        if '--validate-only' not in sys.argv:run('moved-'+name,[package/'bin/AzurePlayer.exe','--project',package/'game/project.azureproject','--resource-root',package/'share/AzureRender','--runtime-report',report,'--capture-dir',capture,'--capture-frames','3','--capture-fps','4'],cwd=package,env=env)
        data=json.loads(report.read_text(encoding='utf-8'));log=(out/('moved-'+name+'.log')).read_text(encoding='utf-8')
        assert data['audioStarts']==1 and not data['presentationErrors'] and not data['scriptErrors'],data
        assert all(any(node['id']==name for node in data['nodes']) for name in ('authored-model','authored-music'))
        assert all(text not in log.lower() for text in ('vuid-','validation error','sync-hazard'))
        assert 'Allocator after unload: buffers=0 images=0' in log and 'VK_LAYER_KHRONOS_validation' in log
        cap=json.loads((capture/'gpu_capabilities.json').read_text(encoding='utf-8'));assert ('Intel' if device=='intel' else 'NVIDIA') in cap['device_name']
        from PIL import Image
        image=Image.open(capture/'frame_000000.png').convert('RGB');assert any(hi-lo>30 for lo,hi in image.getextrema()),'Authored geometry must appear in the actual capture'
        rows.append({'device':device,'workflow':workflow,'packageFiles':len(manifest['files']),'audioStarts':data['audioStarts'],'animationFrames':data['animationFrames'],'capabilities':cap});build_game.verify_package(package)
licenses={}
for name in ('miniaudio','stb','libvorbis','libogg','libflac','mp3lame','assimp','tiff','libwebp','openexr','zlib','directxtex','directxmath','ktx','zstd','opus','opusfile'):
    path=engine/'share/AzureRender/licenses'/(name+'-LICENSE.txt');assert path.is_file();licenses[name]=hashlib.sha256(path.read_bytes()).hexdigest()
notice=engine/'share/AzureRender/licenses/compressed-audio'
for path in (root/'docs/licenses/compressed-audio').glob('*'):
    if path.name!='.gitattributes':assert (notice/path.name).read_bytes()==path.read_bytes(),path.name
summary={'passed':True,'fileCount':json.loads((out/'install-manifest.json').read_text(encoding='utf-8'))['file_count'],'rows':rows,'sourceUnavailable':True,'installationMoved':True,'isolatedEnvironment':True,'licenses':licenses}
(out/'delivery-summary.json').write_text(json.dumps(summary,indent=2)+'\n',encoding='utf-8');print('Opus and M4A authored projects, dual-device moved packages, validation and complete licenses passed')
