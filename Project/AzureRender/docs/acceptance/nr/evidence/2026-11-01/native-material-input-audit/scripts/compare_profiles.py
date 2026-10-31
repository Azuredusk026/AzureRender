import copy,json,sys
from pathlib import Path
root=Path.cwd();run=Path(__file__).resolve().parent;sys.path.insert(0,str(root/'tools'))
from retarget_animation import read_glb
source=root/'build/runs/20261031-135416-character-portrait-recovery-0773ca/original-reference/project'
document,_=read_glb(source/'native-character/outline-converted.glb')
old={m['name']:copy.deepcopy(m.get('extras',{}).get('azureRenderMaterial',{})) for m in document['materials']}
hair=json.loads((source/'high/reference.azurematerial').read_text())['surface']['materials'][0]
old[hair['name']]=copy.deepcopy(hair['extras']['azureRenderMaterial'])
current=root/'build/runs/20261031-231334-receiver-directional-light-911ab7/project/assets/restored-character-materials'
def flatten(value,prefix=''):
    if isinstance(value,dict):
        return {k:v for key,child in value.items() for k,v in flatten(child,prefix+'.'+key if prefix else key).items()}
    return {prefix:value}
reports=[]
for path in sorted(current.glob('*.azurematerial')):
    material=json.loads(path.read_text())['surface']['materials'][0];name=material['name']
    a=flatten(old[name]);b=flatten(material.get('extras',{}).get('azureRenderMaterial',{}))
    differences={k:{'reference':a.get(k),'current':b.get(k)} for k in sorted(a.keys()|b.keys()) if a.get(k)!=b.get(k)}
    # Texture table indices belong to separate complete documents.
    differences={k:v for k,v in differences.items() if not k.endswith(('Texture','.texture','.rampTexture','.parameterTexture','.dataTexture'))}
    reports.append({'material':name,'differences':differences})
result={'status':'profile-differences-measured','scope':'Original accepted portrait material profile versus current thirteen complete documents. Texture table indices are excluded; the original sparse body/face rim override is recorded separately.',
    'originalSparseOverrides':{'bodyAndFace':{'rimAmount':0}},'cases':reports}
(run/'profile-differences.json').write_text(json.dumps(result,indent=2)+'\n')
print(json.dumps(result,indent=2))
