from pathlib import Path
import json,os,sys
import numpy as np
root=Path.cwd().resolve();sys.path.insert(0,str(root/'tools'))
from test_character_finish import capture,pixels
from run_playable_long_run import isolated_environment
out=root/'build/g19-dependency-evaluation-2026-10-12';fixtures=out/'image-fixtures';exe=out/'install-moved/bin/AzureRender.exe'
inventory=json.loads((root/'docs/acceptance/p1/evidence/2026-10-05/driver-inventory.json').read_text(encoding='utf-8'));intel=next(x for x in inventory if x['DriverDesc']=='Intel(R) UHD Graphics')
base=isolated_environment();os.environ.clear();os.environ.update(base)
os.environ.update(VK_INSTANCE_LAYERS='VK_LAYER_KHRONOS_validation',VK_LAYER_VALIDATE_SYNC='1',VK_LOADER_DEBUG='error,layer',VK_DRIVER_FILES=intel['VulkanDriverName'],VK_ICD_FILENAMES=intel['VulkanDriverName'])
folder=out/'lossless-repeat';folder.mkdir(exist_ok=True);rows=[]
for i in range(3):
    results={}
    for name in ('png','webp'):
        result=capture(exe,fixtures/(name+'-'+chr(0x56fe)+'.gltf'),folder,name+str(i),'full-body-front','neutral-material',frames=3)
        results[name]=[pixels(result/f'frame_{frame:06d}.png') for frame in range(3)]
    for frame in range(3):
        delta=np.abs(results['png'][frame]-results['webp'][frame]);row={'run':i,'frame':frame,'differentPixels':int(np.any(delta,axis=2).sum()),'maximumDifference':int(delta.max())};rows.append(row);print(row,flush=True)
(folder/'summary.json').write_text(json.dumps(rows,indent=2)+'\n',encoding='utf-8')
