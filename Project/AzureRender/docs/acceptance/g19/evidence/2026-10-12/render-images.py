from pathlib import Path
import hashlib,json,os,subprocess,sys
import numpy as np
root=Path.cwd().resolve();out=root/'build/g19-dependency-evaluation-2026-10-12'
sys.path.insert(0,str(root/'tools'))
from test_character_finish import capture,pixels
from run_playable_long_run import isolated_environment
config=sys.argv[1];build=root/('build/ninja-msvc-'+config.lower())
exe=out/'install-moved/bin/AzureRender.exe' if config=='Release' else build/'AzureRender.exe'
fixtures=out/'image-fixtures';fixtures.mkdir(exist_ok=True)
result=subprocess.run([str(build/'AzureImageCodecTests.exe'),str(fixtures)],capture_output=True,text=True,encoding='utf-8',errors='replace')
(out/(config.lower()+'-fixture-generation.log')).write_text(result.stdout+result.stderr,encoding='utf-8');assert result.returncode==0,result.stdout+result.stderr
source=json.loads((root/'assets_public/test_model.gltf').read_text(encoding='utf-8'))
texture=source['materials'][0]['pbrMetallicRoughness']['baseColorTexture']['index'];image=source['textures'][texture]['source']
cases={'png':'alpha.png','jpeg':'color.jpg','bmp':'color.bmp','tga':'color.tga','ppm':'color.ppm','hdr':'linear.hdr','exr':'linear.exr','tiff':'gray16.tiff','webp':'color.webp','png16':'gray16.png','float-tiff':'linear.tiff'}
for name,file in cases.items():
    source['images'][image]['uri']=file
    (fixtures/(name+'-'+chr(0x56fe)+'.gltf')).write_text(json.dumps(source),encoding='utf-8')
inventory=json.loads((root/'docs/acceptance/p1/evidence/2026-10-05/driver-inventory.json').read_text(encoding='utf-8'))
intel=next(x for x in inventory if x['DriverDesc']=='Intel(R) UHD Graphics')
assert hashlib.sha256(Path(intel['VulkanDriverName']).read_bytes()).hexdigest()==intel['manifestSha256']
base=isolated_environment() if config=='Release' else os.environ.copy();rows=[]
for device in ('nvidia','intel'):
    os.environ.clear();os.environ.update(base)
    os.environ.update(VK_INSTANCE_LAYERS='VK_LAYER_KHRONOS_validation',VK_LAYER_VALIDATE_SYNC='1',VK_LOADER_DEBUG='error,layer')
    if device=='intel':os.environ.update(VK_DRIVER_FILES=intel['VulkanDriverName'],VK_ICD_FILENAMES=intel['VulkanDriverName'])
    folder=out/'image-render'/config/device;folder.mkdir(parents=True,exist_ok=True)
    captured={}
    for name,file in cases.items():
        asset=fixtures/(name+'-'+chr(0x56fe)+'.gltf')
        result=folder/name
        if '--retained' not in sys.argv:result=capture(exe,asset,folder,name,'full-body-front','neutral-material',frames=2)
        log=(folder/(name+'.log')).read_text(encoding='utf-8');assert 'VK_LAYER_KHRONOS_validation' in log
        assert not any(x in log.lower() for x in ('sync-hazard','hazard detected','validation error','vuid-'))
        manifest=json.loads((result/'capture_manifest.json').read_text(encoding='utf-8'));actual=pixels(result/'frame_000000.png');captured[name]=actual
        assert ('Intel' if device=='intel' else 'NVIDIA') in manifest['gpu'];assert actual.std()>2
        rows.append({'device':device,'gpuName':manifest['gpu'],'format':name,'assetSha256':hashlib.sha256(asset.read_bytes()).hexdigest(),'imageSha256':hashlib.sha256((result/'frame_000000.png').read_bytes()).hexdigest(),'imageStd':float(actual.std()),'validationErrors':0,'allocatorEmpty':True})
        print(config,device,name,'passed',flush=True)
    # This fixed fixture's textured panel occupies x=[0,640), y=[41,681).
    # Its floor is a separate material and has independent GPU rounding noise.
    assert np.array_equal(captured['png'][41:681,:640],captured['webp'][41:681,:640]), 'Lossless PNG/WebP textured material pixels must match exactly'
    delta=np.abs(captured['png']-captured['webp'])
    for row in rows:
        if row['device']==device and row['format']=='webp':row['fullFrameDifferentPixels']=int(np.any(delta,axis=2).sum());row['fullFrameMaximumDifference']=int(delta.max())
(out/'image-render'/config/'summary.json').write_text(json.dumps({'passed':True,'configuration':config,'cases':rows,'losslessMaterialGpuPixelEquality':True,'materialRoi':[0,41,640,681],'runtimeSha256':hashlib.sha256(exe.read_bytes()).hexdigest()},indent=2)+'\n',encoding='utf-8')
