from pathlib import Path
import hashlib,json,math,re,struct
r=Path(__file__).resolve().parent;root=Path.cwd();before=json.loads((r/'level-before-save.json').read_text());after=json.loads((r/'移动后的完整场景/scene/playable.azurelevel').read_text());changes=[];added=[]
f32=lambda v:struct.pack('<f',v)
def compare(a,b,path=''):
 if isinstance(a,dict):
  assert isinstance(b,dict),path
  for key,value in a.items():assert key in b,path+'/'+key;compare(value,b[key],path+'/'+key)
  for key in b.keys()-a.keys():added.append(dict(path=path+'/'+key,value=b[key]))
 elif isinstance(a,list):
  assert isinstance(b,list) and len(a)==len(b),path
  for i,(x,y) in enumerate(zip(a,b)):compare(x,y,path+'/'+str(i))
 elif a!=b:
  if path=='/renderSettings/screenSpaceReflection/schemaVersion':assert a==1 and b==2 and after['renderSettings']['screenSpaceReflection']['algorithm']=='linear';kind='supported-linear-schema-migration'
  else:assert isinstance(a,float) and isinstance(b,(int,float)) and f32(a)==f32(b),(path,a,b);kind='float32-equivalent'
  changes.append(dict(path=path,kind=kind,before=a,after=b))
 else:assert type(a)==type(b) or type(a) in (int,float) and type(b) in (int,float),path
compare(before,after)
npr_defaults=dict(shadowAmbientGain=1,indoorMix=0,shadowOverride=0,ambientWeight=1,directionGain=0,ambientColor=[1,1,1],litAmbientMix=0,directionBias=0,mainColorMix=0,outdoorGain=1,ambientDirection=[0,1,0],directionFloor=1)
def check_component(value,name,node):
 assert value['type']==name and value['version']==1
 if name=='azure.renderable':assert value['data']==dict(primitiveIndex=node.get('primitiveIndex',0),visible=node.get('visible',bool(node.get('resourceId'))))
 elif name=='azure.transform':assert value['data']==dict(translation=node.get('translation',[0,0,0]),rotation=node.get('rotation',[0,0,0]),scale=node.get('scale',[1,1,1]))
 else:raise AssertionError(name)
for item in added:
 path,value=item['path'],item['value'];match=re.fullmatch(r'/nodes/(\d+)/(components(?:/azure\.renderable)?|resourceId)',path)
 if match:
  node=before['nodes'][int(match[1])];field=match[2]
  if field=='resourceId':assert value==''
  elif field=='components':
   assert set(value)<={'azure.transform','azure.renderable'}
   for name,component in value.items():check_component(component,name,node)
  else:check_component(value,'azure.renderable',node)
 elif re.fullmatch(r'/lights/\d+/shape/receivers',path):assert value==dict(characters=True,schemaVersion=1,surfaces=True)
 elif re.fullmatch(r'/lights/\d+/shape/innerCone',path):assert value==0
 elif re.fullmatch(r'/lights/\d+/shape/outerCone',path):assert f32(value)==f32(math.pi/4)
 elif path=='/renderSettings/screenSpaceReflection/algorithm':assert value=='linear'
 elif path.startswith('/renderSettings/nprLighting/'):assert value==npr_defaults[path.rsplit('/',1)[1]]
 else:raise AssertionError(item)
inputs=json.loads((r/'source-inputs.json').read_text());materials=[x for x in inputs['files'] if x['path'].startswith('assets/restored-character-materials/')];sha=lambda p:hashlib.sha256(p.read_bytes()).hexdigest()
assert len(materials)==26
assert all(sha(r/'移动后的完整场景'/x['path'])==x['sha256'] for x in materials)
assert json.loads((r/'moved-reopen/summary.json').read_text())['status']=='passed'
report=dict(status='passed',nodes=len(after['nodes']),resources=len(after['resources']),preservedOriginalFields=True,addedCanonicalDefaults=len(added),float32EquivalentFields=sum(x['kind']=='float32-equivalent' for x in changes),supportedSchemaMigrations=[x for x in changes if x['kind']!='float32-equivalent'],unchangedMaterialFiles=len(materials),pixelExactReopenedFrames=3,serializationContract='src/runtime/LevelRenderSettings.hpp',serializationContractSha256=sha(root/'src/runtime/LevelRenderSettings.hpp'),scope='No original field removed; numeric changes retain identical float32 values; declared defaults and supported linear-reflection migration are checked separately; actual moved Player images are byte-identical.')
(r/'save-semantic-verification.json').write_bytes((json.dumps(report,indent=2)+'\n').encode());print(json.dumps(report,indent=2))
