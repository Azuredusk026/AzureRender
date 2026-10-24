"""Measure generated cutout mips through Player and independent phase tiles."""
import argparse
import base64
import hashlib
import json
from pathlib import Path
import re
import struct
import subprocess
import tempfile
import uuid
import numpy as np
from PIL import Image
from native_render.light_lab import generate


def fixture(root,shape,level,opaque=False,nearest=False):
    generate(root,'dark')
    y,x=np.mgrid[:512,:512]
    alpha=(x+y*.19)%32<9 if shape=='slant' else ((x-255.5)**2+(y-255.5)**2>=153**2)&((x-255.5)**2+(y-255.5)**2<169**2)
    pixels=np.full((512,512,4),255,dtype='uint8');pixels[:,:,3]=alpha*255
    Image.fromarray(pixels,'RGBA').save(root/'coverage.png')
    (root/'coverage.png.azmeta').write_text(json.dumps({'schemaVersion':1,'id':str(uuid.uuid4()),'dependencies':[],
        'settings':{'texture':{'filter':'nearest' if nearest else 'linear','mipFilter':'nearest','anisotropy':1,'wrapU':'repeat','wrapV':'repeat','mipBias':0}}}))
    side=512>>level;extent=8*side+18
    positions=[];uvs=[];indices=[]
    for row in range(8):
        for column in range(8):
            left=-extent/2+2+column*(side+2);bottom=-extent/2+2+row*(side+2)
            start=len(positions);positions.extend(((left,bottom,0),(left+side,bottom,0),(left+side,bottom+side,0),(left,bottom+side,0)))
            u=(column+.5)/8/side;v=(row+.5)/8/side
            uvs.extend(((u,v),(u+1,v),(u+1,v+1),(u,v+1)));indices.extend(start+i for i in (0,1,2,0,2,3))
    arrays=[struct.pack('<'+'f'*len(values),*values) for values in ([v for p in positions for v in p],[0,0,1]*len(positions),[v for p in uvs for v in p])]
    arrays.append(struct.pack('<'+'H'*len(indices),*indices));offset=0;views=[]
    for data in arrays:views.append({'buffer':0,'byteOffset':offset,'byteLength':len(data)});offset+=len(data)
    asset={'asset':{'version':'2.0','generator':'Azure alpha footprint fixture','extras':{'license':'CC0-1.0'}},
        'scene':0,'scenes':[{'nodes':[0]}],'nodes':[{'mesh':0}],
        'buffers':[{'byteLength':offset,'uri':'data:application/octet-stream;base64,'+base64.b64encode(b''.join(arrays)).decode()}],
        'bufferViews':views,'accessors':[{'bufferView':i,'componentType':5123 if i==3 else 5126,
            'count':len(indices) if i==3 else len(positions),'type':('VEC3','VEC3','VEC2','SCALAR')[i],
            **({'min':np.min(positions,0).tolist(),'max':np.max(positions,0).tolist()} if i==0 else {})} for i in range(4)],
        'images':[{'uri':'coverage.png'}],'textures':[{'source':0}],
        'materials':[{'name':'Coverage','doubleSided':True,'alphaMode':'OPAQUE' if opaque else 'MASK','alphaCutoff':.5,
            'pbrMetallicRoughness':{'baseColorTexture':{'index':0},'baseColorFactor':[.5,.5,.5,1],'metallicFactor':0,'roughnessFactor':1}}],
        'meshes':[{'primitives':[{'attributes':{'POSITION':0,'NORMAL':1,'TEXCOORD_0':2},'indices':3,'material':0}]}]}
    (root/'plane.gltf').write_text(json.dumps(asset))
    path=root/'level.azurelevel';document=json.loads(path.read_text())
    document['renderSettings'].update(exposure=0,contrast=1,saturation=1)
    next(n for n in document['nodes'] if n['id']=='camera')['components']['azure.camera']['data']['orthographicHeight']=extent
    path.write_text(json.dumps(document))
    return extent,side,1. if opaque else float(alpha.mean())


def run(player,output,validation,selected=None):
    output.mkdir(parents=True,exist_ok=False);digest=hashlib.sha256(player.read_bytes()).hexdigest()
    report=dict(schemaVersion=1,status='running',playerSha256=digest,captures=[])
    report['shaders']={p.name:hashlib.sha256(p.read_bytes()).hexdigest() for p in (player.parent/'shaders').glob('*.spv')}
    try:
        default=None
        cases=[('opaque','slant',4,True,False,False)]+[(f'{shape}-{level}',shape,level,False,False,False) for shape in ('slant','ring') for level in (2,3,4)]+[('slant-4-fixed','slant',4,False,True,False),('slant-4-nearest','slant',4,False,False,True)]
        if selected:
            assert set(selected)<=set(row[0] for row in cases)
            cases=[row for row in cases if row[0] in selected]
        for name,shape,level,opaque,fixed,nearest in cases:
            print(name,flush=True);root=output/name;extent,side,target=fixture(root,shape,level,opaque,nearest)
            command=[str(player),'--project',str(root/'lab.azureproject'),'--width',str(extent),'--height',str(extent),
                '--capture-dir',str(root/'frames'),'--capture-frames','1','--fixed-frame-step','--qa-camera','scene','--qa-light','scene','--qa-isolation','albedo']
            if fixed:command+=['--disable-bindless','--disable-compute-skinning','--disable-parallel-recording']
            (root/'command.json').write_text(json.dumps(command,indent=2))
            with (root/'player.log').open('w',encoding='utf-8') as log:subprocess.run(command,stdout=log,stderr=subprocess.STDOUT,check=True,timeout=180)
            log=(root/'player.log').read_text(encoding='utf-8',errors='replace')
            assert not re.search(r'VUID-|Validation (?:Error|Warning)|\[error\]',log)
            assert 'Allocator after unload: buffers=0 images=0' in log
            assert ('Validation layer: enabled' in log)==(validation=='enabled')
            image=root/'frames/frame_000000.png';pixels=np.asarray(Image.open(image).convert('RGB')).astype(int)
            assert pixels.shape==(extent,extent,3)
            geometry=np.zeros((extent,extent),dtype=bool)
            for y in range(8):
                for x in range(8):geometry[2+y*(side+2):2+y*(side+2)+side,2+x*(side+2):2+x*(side+2)+side]=True
            assert int(geometry.sum())==64*side*side
            foreground=np.max(np.abs(pixels-188),axis=2)<=1
            background=np.max(np.abs(pixels-pixels[0,0]),axis=2)<=1
            assert np.all(foreground|background),'Unexpected shading or filtered coverage in the unfiltered albedo probe'
            assert not np.any(foreground[~geometry]),'Projected tile mask differs from rasterized geometry'
            coverage=float(foreground[geometry].mean());error=abs(coverage/target-1)
            manifest=json.loads((root/'frames/capture_manifest.json').read_text(encoding='utf-8-sig'))
            assert manifest['antiAliasing']==0
            row=dict(name=name,mip=level,phaseTiles=64,extent=extent,coveredSamples=int(foreground[geometry].sum()),
                samples=int(geometry.sum()),coverage=coverage,targetCoverage=target,relativeError=error,imageSha256=hashlib.sha256(image.read_bytes()).hexdigest(),
                sourceImageSha256=hashlib.sha256((root/'coverage.png').read_bytes()).hexdigest(),withinLevelFilter='nearest' if nearest else 'linear',
                modelSha256=hashlib.sha256((root/'plane.gltf').read_bytes()).hexdigest(),metadataSha256=hashlib.sha256((root/'coverage.png.azmeta').read_bytes()).hexdigest(),
                levelSha256=hashlib.sha256((root/'level.azurelevel').read_bytes()).hexdigest(),materialTextureCache=manifest['materialTextureCache'])
            report['captures'].append(row)
            assert error<=.02,row
            if name=='slant-4':default=pixels
            if fixed and default is not None:assert np.array_equal(default,pixels),'Fixed and indexed descriptors must preserve coverage'
        assert hashlib.sha256(player.read_bytes()).hexdigest()==digest
        assert report['shaders']=={p.name:hashlib.sha256(p.read_bytes()).hexdigest() for p in (player.parent/'shaders').glob('*.spv')}
        report['status']='passed'
    except Exception as error:report.update(status='failed',error=str(error));raise
    finally:(output/'summary.json').write_text(json.dumps(report,indent=2)+'\n')


if __name__=='__main__':
    parser=argparse.ArgumentParser(__doc__);parser.add_argument('--executable',type=Path,required=True)
    parser.add_argument('--output',type=Path);parser.add_argument('--validation',choices=('enabled','disabled'),required=True)
    parser.add_argument('--case',action='append');args=parser.parse_args()
    if args.output:run(args.executable.resolve(),args.output.resolve(),args.validation,args.case)
    else:
        with tempfile.TemporaryDirectory(prefix='alpha-mips-') as temporary:run(args.executable.resolve(),Path(temporary)/'captures',args.validation,args.case)
