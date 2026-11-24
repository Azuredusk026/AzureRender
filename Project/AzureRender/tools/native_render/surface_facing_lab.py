from pathlib import Path
import sys,json,subprocess,re,argparse,tempfile,numpy as np
from PIL import Image
E=next(p for p in Path(__file__).resolve().parents if (p/'tools/test_cube_probe_gpu.py').exists());sys.path.insert(0,str(E/'tools'))
from test_cube_probe_gpu import fixture,observation_errors,BUDGET
R=Path(__file__).resolve().parent

def prepare(root,mirrored,back,double=True,blend=False):
 project=fixture(root,4,.25);p=root/'assets/surface.gltf';j=json.loads(p.read_text());j['materials'][0]['doubleSided']=double
 if blend:j['materials'][0]['alphaMode']='BLEND'
 p.write_text(json.dumps(j));p=root/'assets/scene.azurelevel';j=json.loads(p.read_text());node=next(n for n in j['nodes'] if n['id']=='surface');node['components']['azure.transform']['data']['scale']=[-1 if mirrored else 1,1,1]
 if back:
  camera=next(n for n in j['nodes'] if n['id']=='camera');camera['components']['azure.transform']['data'].update(translation=[0,0,-3],rotation=[0,180,0])
 p.write_text(json.dumps(j));return project

def cases():
 result=[]
 for blend in [False,True]:
  for mirrored,back in [(False,False),(True,False),(False,True),(True,True)]:
   result.append(dict(name=('blend-' if blend else 'opaque-')+('mirrored-' if mirrored else 'ordinary-')+('back' if back else 'front'),mirrored=mirrored,back=back,blend=blend))
 result.extend([dict(name='single-mirrored',mirrored=True,back=False,double=False),dict(name='parent-double-mirror',mirrored=True,back=False,parentMirror=True),dict(name='msaa-mirrored-front',mirrored=True,back=False,msaa=True),dict(name='msaa-mirrored-back',mirrored=True,back=True,msaa=True)])
 return result

def run(player,output,selected=None):
 output.mkdir(parents=True,exist_ok=False);receipts=[]
 for case in cases():
  if selected and selected!=case['name']:continue
  for indexed in [True,False]:
   folder=output/(case['name']+('-indexed' if indexed else '-fixed'));project=prepare(folder/'project',case['mirrored'],case['back'],case.get('double',True),case.get('blend',False))
   p=folder/'project/assets/scene.azurelevel';level=json.loads(p.read_text())
   if case.get('parentMirror'):
    node=next(n for n in level['nodes'] if n['id']=='surface');node['parentId']='mirror-parent';level['nodes'].append(dict(id='mirror-parent',components={'azure.transform':{'type':'azure.transform','version':1,'data':{'scale':[-1,1,1]}}}))
   if case.get('msaa'):level['renderSettings'].update(schemaVersion=19,antiAliasing=4)
   p.write_text(json.dumps(level,indent=2))
   command=[str(player.resolve()),'--project',str(project.resolve()),'--width','128','--height','128','--capture-dir',str((folder/'frames').resolve()),'--capture-frames','1','--fixed-frame-step','--qa-camera','scene','--qa-light','scene','--qa-isolation','shaded-normal']
   if not indexed:command.append('--disable-bindless')
   (folder/'command.json').write_text(json.dumps(command));startup=None
   if hasattr(subprocess,'STARTUPINFO'):startup=subprocess.STARTUPINFO();startup.dwFlags|=subprocess.STARTF_USESHOWWINDOW;startup.wShowWindow=0
   with (folder/'player.log').open('w') as log:subprocess.run(command,stdout=log,stderr=subprocess.STDOUT,check=True,timeout=180,startupinfo=startup)
   log=(folder/'player.log').read_text(errors='replace');assert not re.search(r'VUID-|Validation (?:Error|Warning)|\[error\]',log);assert 'Allocator after unload: buffers=0 images=0' in log
   image=np.asarray(Image.open(folder/'frames/frame_000000.png').convert('RGB'));blue=image[62:66,62:66,2];passed=bool(np.all(blue<5) if case['back'] else np.all(blue>250))
   mirrored=case['mirrored']^case.get('parentMirror',False);back=case['back']
   normal=np.asarray([-1/255 if mirrored else 1/255,-1/255 if back else 1/255,-1 if back else 1],dtype=float);normal/=np.linalg.norm(normal)
   errors=observation_errors(image[62:66,62:66],(normal+1)*.5)
   assert errors['maximumQuantizationIntervalLinearError']<=BUDGET,(case,errors)
   row=dict(name=case['name'],indexed=indexed,minimumBlue=int(blue.min()),maximumBlue=int(blue.max()),passed=passed,validationErrors=0,resourcesReleased=True,**errors);receipts.append(row)
   (output/'summary.json').write_text(json.dumps(receipts,indent=2));assert passed,(case,row)
   if not indexed:assert np.array_equal(image,np.asarray(Image.open(output/(case['name']+'-indexed')/'frames/frame_000000.png').convert('RGB')))
 return receipts
if __name__=='__main__':
 parser=argparse.ArgumentParser(description='Verify two-sided authored facing under mirrored transforms');parser.add_argument('--player',type=Path,required=True);parser.add_argument('--output',type=Path);parser.add_argument('--case',choices=[c['name'] for c in cases()]);args=parser.parse_args()
 if args.output:result=run(args.player,args.output,args.case)
 else:
  with tempfile.TemporaryDirectory(prefix='azure-surface-facing-') as temporary:result=run(args.player,Path(temporary)/'result',args.case)
 print(json.dumps(dict(status='passed',captures=len(result))))
