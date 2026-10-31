import base64,hashlib,json,sys
from pathlib import Path
from io import BytesIO
import numpy as np
from PIL import Image
root=Path.cwd();run=Path(__file__).resolve().parent;sys.path.insert(0,str(root/'tools'))
from retarget_animation import read_glb
oldroot=root/'build/runs/20261031-135416-character-portrait-recovery-0773ca/original-reference/project'
newroot=root/'build/runs/20261031-231334-receiver-directional-light-911ab7/project'
old,chunks=read_glb(oldroot/'native-character/outline-converted.glb')
blob=next(v for k,v in chunks if k==0x004e4942)
metas={json.loads(p.read_text(encoding='utf-8-sig'))['id']:p.with_suffix('') for p in newroot.rglob('*.azmeta') if '.azure' not in p.relative_to(newroot).parts}
def old_image(index):
    im=old['images'][old['textures'][index]['source']]
    if 'uri' in im:
        uri=im['uri'];raw=base64.b64decode(uri.split(',')[1]) if uri.startswith('data:') else (oldroot/'native-character'/uri).read_bytes()
    else:
        view=old['bufferViews'][im['bufferView']];raw=blob[view.get('byteOffset',0):view.get('byteOffset',0)+view['byteLength']]
    return np.array(Image.open(BytesIO(raw)).convert('RGBA')),raw
def slots(material):
    out={}
    def walk(obj,path=''):
        if not isinstance(obj,dict):return
        for k,v in obj.items():
            name=path+'.'+k if path else k
            if k=='nativeCharacterProfileSource' or k=='nativeOpaqueSource':continue
            if isinstance(v,dict) and 'index' in v and k.endswith('Texture'):out[name]=v['index']
            elif isinstance(v,int) and (k.endswith('Texture') or k=='texture'):out[name]=v
            elif isinstance(v,dict):walk(v,name)
    walk(material)
    return out
cases=[]
for path in sorted((newroot/'assets/restored-character-materials').glob('*.azurematerial')):
    doc=json.loads(path.read_text());mat=doc['surface']['materials'][0];prior=next(m for m in old['materials'] if m['name']==mat['name'])
    a=slots(prior);b=slots(mat)
    for slot in sorted(a.keys()&b.keys()):
        row={'material':mat['name'],'slot':slot}
        try:
            oldpixels,oldraw=old_image(a[slot]);source=metas[doc['textures'][str(b[slot])]['asset']]
            raw=source.read_bytes();pixels=np.array(Image.open(BytesIO(raw)).convert('RGBA'))
            row.update(source=str(source.relative_to(newroot)),referenceSize=list(oldpixels.shape),currentSize=list(pixels.shape),referenceSha256=hashlib.sha256(oldraw).hexdigest(),currentSha256=hashlib.sha256(raw).hexdigest())
            if oldpixels.shape==pixels.shape:
                delta=lambda p:np.abs(oldpixels.astype(int)-p.astype(int))
                d=delta(pixels);f=delta(pixels[::-1])
                row.update(maximumChannelError=int(d.max()),meanChannelError=float(d.mean()),flippedMaximumChannelError=int(f.max()),flippedMeanChannelError=float(f.mean()))
                if slot.endswith('shadowColorLut.texture'):
                    pa=prior['extras']['azureRenderMaterial']['shadowColorLut'];pb=mat['extras']['azureRenderMaterial']['shadowColorLut']
                    av=oldpixels[::-1] if pa.get('flipV') else oldpixels;bv=pixels[::-1] if pb.get('flipV') else pixels
                    diff=np.abs(av.astype(int)-bv.astype(int));row.update(effectiveMaximumChannelError=int(diff.max()),effectiveMeanChannelError=float(diff.mean()))
        except Exception as e:row['error']=str(e)
        cases.append(row)
(run/'texture-audit.json').write_text(json.dumps({'scope':'Decoded level-zero semantic bindings; compressed mip streams retain separate provenance. Hair reference override texture bindings equal embedded source indices.','cases':cases},indent=2)+'\n')
print(json.dumps(cases,indent=2))
