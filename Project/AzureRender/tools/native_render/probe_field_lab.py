"""Exercise ordered probe fields through scene documents and real Player draws."""
import argparse,json,math,struct,subprocess,sys,re,hashlib,tempfile
from pathlib import Path
import numpy as np
from PIL import Image
E=next(p for p in Path(__file__).resolve().parents if (p/'tools/test_cube_probe_gpu.py').exists())
sys.path.insert(0,str(E/'tools'))
from test_cube_probe_gpu import fixture,observation_errors,BUDGET
COLORS=np.asarray([[.125,.25,.5],[.5,.125,.25],[.25,.5,.125]])
def volume(layer=1,fade=1):
 return dict(luminanceSH=[0,0,0,1],halfExtents=[2,2,2],worldToLocal=[[1,0,0,0],[0,1,0,0],[0,0,1,0]],inverseBlendWidths=[1,1,1],arrayLayer=layer,intensity=1,fade=fade,shape='box',projection=True)
def cases():
 return [dict(name='inner',wanted=COLORS[1]),dict(name='boundary',wanted=(COLORS[0]+COLORS[1])*.5),
  dict(name='nested',wanted=(COLORS[1]+COLORS[2])*.5),dict(name='priority',wanted=COLORS[2]),
  dict(name='fallback',wanted=COLORS[0]),dict(name='adapted',wanted=COLORS[1]*.5),
  dict(name='snapshot',wanted=COLORS[1],snapshot=True),dict(name='msaa4',wanted=COLORS[1],msaa=True)]
def prepare(root,case):
 project=fixture(root,4,.25);p=root/'assets/scene.azurelevel';level=json.loads(p.read_text());v=volume();field=dict(schemaVersion=1,probes=[v],defaultLayer=0,defaultLuminanceSH=[0,0,0,1],adaptation=0,intensity=1,worldToField=[[1,0,0],[0,1,0],[0,0,1]])
 if case['name']=='boundary':v['worldToLocal'][2][3]=1.5
 if case['name']=='nested':v['fade']=.5;field['probes'].append(volume(2))
 if case['name']=='priority':field['probes'].insert(0,volume(2))
 if case['name']=='fallback':v['worldToLocal'][2][3]=3
 if case['name']=='adapted':field['adaptation']=1;v['luminanceSH']=[0,0,0,2];v['intensity']=1
 if case['name']=='adapted':
  cells=struct.pack('<16f',0,0,0,1,0,0,0,1,0,0,0,1,1,0,0,0)*8
  (root/'assets/unit.aziv').write_bytes(b'AZIV0001'+struct.pack('<4f3I',-4,-4,-4,8,2,2,2)+cells)
  level['irradianceVolume']=dict(version=1,asset='assets:/unit.aziv',intensity=1,enabled=True)
 level['reflectionProbe']=dict(version=4,texture='assets:/probe.azoa',field=field)
 if case.get('snapshot'):level['renderSettings']['screenSpaceReflection']=dict(schemaVersion=1,enabled=True,maxSteps=8)
 if case.get('msaa'):level['renderSettings'].update(schemaVersion=19,antiAliasing=4)
 p.write_text(json.dumps(level,indent=2))
 payload=bytearray(b'AZOA0001'+struct.pack('<4I4f2I',4,4,3,3,1,1,0,0,1,1))
 for mip in range(3):
  for color in COLORS:
   half=[struct.unpack('<H',struct.pack('<e',float(x)))[0] for x in color];word=(half[0]>>4)|((half[1]>>4)<<11)|((half[2]>>5)<<22)
   payload.extend(struct.pack('<I',word)*max(1,4>>mip)**2)
 (root/'assets/probe.azoa').write_bytes(payload)
 return project
def run(player,output,selected=None):
 output.mkdir(parents=True,exist_ok=False);report=dict(schemaVersion=1,status='running',playerSha256=hashlib.sha256(player.read_bytes()).hexdigest(),budget=BUDGET,captures=[])
 for case in cases():
  if selected and case['name']!=selected:continue
  for indexed in [True,False]:
   folder=output/(case['name']+('-indexed' if indexed else '-fixed'));project=prepare(folder/'project',case)
   command=[str(player.resolve()),'--project',str(project.resolve()),'--width','128','--height','128','--capture-dir',str((folder/'frames').resolve()),'--capture-frames','1','--fixed-frame-step','--qa-camera','scene','--qa-light','scene','--qa-isolation','reflection']
   if not indexed:command.append('--disable-bindless')
   (folder/'command.json').write_text(json.dumps(command));startup=None
   if hasattr(subprocess,'STARTUPINFO'):startup=subprocess.STARTUPINFO();startup.dwFlags|=subprocess.STARTF_USESHOWWINDOW;startup.wShowWindow=0
   with (folder/'player.log').open('w') as log:subprocess.run(command,stdout=log,stderr=subprocess.STDOUT,timeout=180,check=True,startupinfo=startup)
   log=(folder/'player.log').read_text(errors='replace');assert not re.search(r'VUID-|Validation (?:Error|Warning)|\[error\]',log)
   assert 'Allocator after unload: buffers=0 images=0' in log
   image=np.asarray(Image.open(folder/'frames/frame_000000.png').convert('RGB'));errors=observation_errors(image[62:66,62:66],case['wanted'])
   assert errors['maximumQuantizationIntervalLinearError']<=BUDGET,(case['name'],errors)
   if not indexed:assert np.array_equal(image,np.asarray(Image.open(output/(case['name']+'-indexed')/'frames/frame_000000.png').convert('RGB')))
   report['captures'].append(dict(name=case['name'],indexed=indexed,expectedLinearRgb=case['wanted'].tolist(),validationErrors=0,resourcesReleased=True,**errors));(output/'summary.json').write_text(json.dumps(report,indent=2))
 report['status']='passed';(output/'summary.json').write_text(json.dumps(report,indent=2));return report
if __name__=='__main__':
 p=argparse.ArgumentParser(description=__doc__);p.add_argument('--player',type=Path,required=True);p.add_argument('--output',type=Path);p.add_argument('--case',choices=[c['name'] for c in cases()]);a=p.parse_args()
 if a.output:r=run(a.player,a.output,a.case)
 else:
  with tempfile.TemporaryDirectory(prefix='azure-probe-field-') as t:r=run(a.player,Path(t)/'result',a.case)
 print(json.dumps(dict(status=r['status'],captures=len(r['captures']),maximumError=max(x['maximumQuantizationIntervalLinearError'] for x in r['captures']))))
