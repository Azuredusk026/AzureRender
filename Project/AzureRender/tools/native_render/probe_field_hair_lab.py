"""Compare constant HDR cube and array sources through integrated hair pipelines."""
import argparse,json,struct,subprocess,re,sys,tempfile,hashlib
from pathlib import Path
import numpy as np
from PIL import Image
E=next(p for p in Path(__file__).resolve().parents if (p/'tools/test_projected_hair_integral_gpu.py').exists());sys.path.insert(0,str(E/'tools'))
from test_projected_hair_integral_gpu import fixture
from native_render.irradiance_clipmap_lab import field as clipmap

def prepare(root,mode,array):
 project=fixture(root,False,True);p=root/'assets/panels.gltf';j=json.loads(p.read_text())
 for i,m in enumerate(j['materials']):m['alphaMode']='BLEND' if mode=='mixed' and i%2 else 'OPAQUE';m['doubleSided']=True
 p.write_text(json.dumps(j));p=root/'assets/scene.azurelevel';level=json.loads(p.read_text())
 if mode=='mirrored':
  for node in level['nodes']:
   if node.get('resourceId'):node['components']['azure.transform']['data']['scale']=[-1,1,1]
 if mode in ('snapshot','msaa4'):level['renderSettings']['screenSpaceReflection']=dict(schemaVersion=1,enabled=True,maxSteps=8)
 if mode=='msaa4':level['renderSettings'].update(schemaVersion=19,antiAliasing=4)
 # Both sources store exact binary HDR constants at every direction and level.
 colors=[2,4,8];payload=b'AZENV002'+struct.pack('<3I',1,1,1)+struct.pack('<4e',*colors,1)*6;(root/'assets/constant.azenv').write_bytes(payload)
 word=(16<<6)|((17<<6)<<11)|((18<<5)<<22)
 (root/'assets/constant.azoa').write_bytes(b'AZOA0001'+struct.pack('<4I4f2I',1,1,1,1,1,1,0,0,1,1)+struct.pack('<I',word))
 if array:
  field=dict(schemaVersion=1,probes=[],defaultLayer=0,defaultLuminanceSH=[0,0,0,1],adaptation=0,intensity=1,worldToField=[[1,0,0],[0,1,0],[0,0,1]])
  level['reflectionProbe']=dict(version=4,texture='assets:/constant.azoa',field=field)
 else:level['reflectionProbe']=dict(version=2,texture='assets:/constant.azenv',center=[0,0,0],halfExtents=[100,100,100],capturePosition=[0,0,0],intensity=1,blendDistance=0,boxProjection=False)
 clipmap(root/'assets/field.azic','baseline');level['irradianceVolume']=dict(version=1,asset='assets:/field.azic',enabled=True,intensity=1/64)
 p.write_text(json.dumps(level,indent=2));return project

def run(player,output):
 output.mkdir(parents=True,exist_ok=False);rows=[]
 for mode in ['opaque','mixed','mirrored','snapshot','msaa4']:
  for indexed in [True,False]:
   reference=None
   for array in [False,True]:
    name=mode+('-indexed' if indexed else '-fixed')+('-array' if array else '-cube');folder=output/name;project=prepare(folder/'project',mode,array)
    command=[str(player.resolve()),'--project',str(project.resolve()),'--width','128','--height','128','--capture-dir',str((folder/'frames').resolve()),'--capture-frames','1','--fixed-frame-step','--qa-camera','scene','--qa-light','scene','--qa-isolation','reflection','--qa-exposure','-2']
    if not indexed:command+=['--disable-bindless','--disable-compute-skinning']
    (folder/'command.json').write_text(json.dumps(command));startup=None
    if hasattr(subprocess,'STARTUPINFO'):startup=subprocess.STARTUPINFO();startup.dwFlags|=subprocess.STARTF_USESHOWWINDOW;startup.wShowWindow=0
    with (folder/'player.log').open('w') as log:subprocess.run(command,stdout=log,stderr=subprocess.STDOUT,check=True,timeout=180,startupinfo=startup)
    log=(folder/'player.log').read_text(errors='replace');assert not re.search(r'VUID-|Validation (?:Error|Warning)|\[error\]',log),name;assert 'Allocator after unload: buffers=0 images=0' in log
    manifest=json.loads((folder/'frames/capture_manifest.json').read_text());expected=0 if mode=='msaa4' else 12 if mode=='mixed' else 8;assert manifest['pixelIntegralPipelineCount']==expected,(name,manifest['pixelIntegralPipelineCount'])
    image=np.asarray(Image.open(folder/'frames/frame_000000.png').convert('RGB'))
    if array:assert np.array_equal(image,reference),(name,'Constant cube and array differ',int(np.abs(image.astype(int)-reference.astype(int)).max()))
    else:reference=image;assert image.max()>0,(name,'Reflection control must be nonzero')
    rows.append(dict(name=name,pixelIntegralPipelineCount=expected,imageSha256=hashlib.sha256(image.tobytes()).hexdigest(),validationErrors=0,resourcesReleased=True));(output/'summary.json').write_text(json.dumps(rows,indent=2))
 return rows
if __name__=='__main__':
 p=argparse.ArgumentParser(description=__doc__);p.add_argument('--player',type=Path,required=True);p.add_argument('--output',type=Path);a=p.parse_args()
 if a.output:r=run(a.player,a.output)
 else:
  with tempfile.TemporaryDirectory(prefix='azure-probe-hair-') as t:r=run(a.player,Path(t)/'result')
 print(json.dumps(dict(status='passed',captures=len(r))))
