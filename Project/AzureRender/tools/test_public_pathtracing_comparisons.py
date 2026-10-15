"""Capture controlled lighting, bounce, and transmission comparisons."""
import argparse,json
from pathlib import Path
import numpy as np
from PIL import Image
from render_pathtracing_demo import run

def compare(install,projects,delivery,output):
    output=Path(output).resolve();results={}
    for name in ('DamagedHelmet','ABeautifulGame','TransmissionRoughnessTest'):
        root=Path(projects)/name;level=root/'assets/room.azurelevel';original=level.read_bytes();gltf=next((root/'assets/model').glob('*.gltf'));model=gltf.read_bytes()
        try:
            scene=json.loads(original)
            if name=='DamagedHelmet':
                for n in scene['nodes']:
                    if n['id'] in ('key','fill','softbox'):
                        pos=n['components']['azure.transform']['data']['translation'];pos[0]=-pos[0]
                effect='mirrored-lighting'
            elif name=='ABeautifulGame':scene['renderSettings']['pathTraceBounces']=1;effect='single-bounce'
            else:
                data=json.loads(model)
                for material in data['materials']:
                    ext=material.get('extensions',{}).get('KHR_materials_transmission')
                    if ext is not None:ext['transmissionFactor']=0
                gltf.write_text(json.dumps(data),encoding='utf-8');effect='opaque-materials'
            level.write_text(json.dumps(scene),encoding='utf-8')
            run(Path(install)/'bin/AzurePlayer.exe',root/'project.azureproject',output/name,(256,),0,(640,480),True)
        finally:level.write_bytes(original);gltf.write_bytes(model)
        baseline=np.asarray(Image.open(Path(delivery)/name/'denoised/captures/samples-256.png').convert('RGB'),dtype=float)/255
        variant=np.asarray(Image.open(output/name/'captures/samples-256.png').convert('RGB'),dtype=float)/255
        difference=float(np.mean(np.abs(baseline-variant)));assert difference>.001,(name,effect,difference)
        results[name]={'effect':effect,'meanAbsoluteDifference':difference,'samples':256,'denoise':True}
        output.mkdir(parents=True,exist_ok=True);(output/'summary.json').write_text(json.dumps(results,indent=2),encoding='utf-8')
        print(name,effect,difference,flush=True)

if __name__=='__main__':
    p=argparse.ArgumentParser(__doc__)
    for arg in ('install','projects','delivery','output'):p.add_argument('--'+arg,required=True)
    a=p.parse_args();compare(a.install,a.projects,a.delivery,a.output)
