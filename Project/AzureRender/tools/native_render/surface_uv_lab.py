"""Compare canonical UV0 and equivalent authored UV1 surfaces on Vulkan."""
from pathlib import Path
import argparse,base64,io,json,re,struct,subprocess,sys,tempfile
import numpy as np
from PIL import Image

E=next(p for p in Path(__file__).resolve().parents if (p/'tools/create_visibility_fixture.py').exists())
sys.path.insert(0,str(E/'tools'))
from create_visibility_fixture import create


def prepare(root,secondary,blend=False,mirror=False,msaa=False):
    project=create(root,normal_map=True,alpha=blend,outline=False,scale=[-1 if mirror else 1,1,1])
    file=root/'assets/surface.gltf';data=json.loads(file.read_text());binary=base64.b64decode(data['buffers'][0]['uri'].split(',')[1])
    primary=[0,0,1,0,1,1,0,1];selected=[0,0,0,1,1,1,1,0]
    primitive=data['meshes'][0]['primitives'][0];primitive['attributes'].pop('TANGENT',None)
    for name,values in [('TEXCOORD_0',primary if secondary else selected),('TEXCOORD_1',selected)]:
        offset=len(binary);binary+=struct.pack('<8f',*values);view=len(data['bufferViews']);data['bufferViews'].append(dict(buffer=0,byteOffset=offset,byteLength=32));index=len(data['accessors']);data['accessors'].append(dict(bufferView=view,componentType=5126,count=4,type='VEC2'));primitive['attributes'][name]=index
    data['buffers'][0]=dict(byteLength=len(binary),uri='data:application/octet-stream;base64,'+base64.b64encode(binary).decode())
    image=Image.new('RGB',(2,2));image.putdata([(255,32,32),(32,255,32),(32,32,255),(255,255,32)]);encoded=io.BytesIO();image.save(encoded,format='PNG');source=len(data.get('images',[]));data.setdefault('images',[]).append(dict(uri='data:image/png;base64,'+base64.b64encode(encoded.getvalue()).decode()));texture=len(data.get('textures',[]));data.setdefault('textures',[]).append(dict(source=source))
    material=data['materials'][0];material['pbrMetallicRoughness']['baseColorTexture']=dict(index=texture,texCoord=int(secondary));material['normalTexture']['texCoord']=int(secondary);file.write_text(json.dumps(data))
    level=root/'assets/scene.azurelevel';data=json.loads(level.read_text())
    if msaa:data['renderSettings'].update(schemaVersion=19,antiAliasing=4)
    level.write_text(json.dumps(data));return project


def run(player,output):
    output.mkdir(parents=True,exist_ok=False);rows=[]
    cases=[('albedo',{},'albedo'),('normal',{},'shaded-normal'),('blend',dict(blend=True),'albedo'),('msaa-mirror',dict(mirror=True,msaa=True),'shaded-normal')]
    for name,options,isolation in cases:
        for indexed in [True,False]:
            reference=None
            for secondary in [False,True]:
                folder=output/(name+('-indexed' if indexed else '-fixed')+('-uv1' if secondary else '-uv0'));project=prepare(folder/'project',secondary,**options)
                command=[str(player.resolve()),'--project',str(project.resolve()),'--width','128','--height','128','--qa-camera','full-body-front','--qa-light','neutral-material','--qa-isolation',isolation,'--qa-effect','outline','--qa-effect-state','disabled','--capture-dir',str((folder/'frames').resolve()),'--capture-frames','1','--fixed-frame-step']
                if not indexed:command.append('--disable-bindless')
                (folder/'command.json').write_text(json.dumps(command));startup=None
                if hasattr(subprocess,'STARTUPINFO'):startup=subprocess.STARTUPINFO();startup.dwFlags|=subprocess.STARTF_USESHOWWINDOW;startup.wShowWindow=0
                with (folder/'player.log').open('w') as log:subprocess.run(command,cwd=E,stdout=log,stderr=subprocess.STDOUT,check=True,timeout=180,startupinfo=startup)
                log=(folder/'player.log').read_text(errors='replace');assert not re.search(r'VUID-|Validation (?:Error|Warning)|\[error\]',log);assert 'Allocator after unload: buffers=0 images=0' in log
                image=np.asarray(Image.open(folder/'frames/frame_000000.png').convert('RGB'));center=image[48:80,48:80]
                if isolation=='shaded-normal':assert np.all(center[:,:,2]>180),(name,'normal reference absent')
                else:assert np.std(center.astype(float))>15,(name,'colored reference absent')
                if not secondary:reference=image
                else:assert np.array_equal(image,reference),(name,indexed,'UV1 differs from canonical reference')
                row=dict(name=name,indexed=indexed,secondary=secondary,validationErrors=0,resourcesReleased=True);rows.append(row);(output/'summary.json').write_text(json.dumps(rows,indent=2));print(row,flush=True)
    for name,_,_ in cases:
        for suffix in ['uv0','uv1']:
            a=np.asarray(Image.open(output/(name+'-indexed-'+suffix)/'frames/frame_000000.png').convert('RGB'))
            b=np.asarray(Image.open(output/(name+'-fixed-'+suffix)/'frames/frame_000000.png').convert('RGB'))
            assert np.array_equal(a,b),(name,suffix,'descriptor paths differ')
    return rows


if __name__=='__main__':
    parser=argparse.ArgumentParser(__doc__);parser.add_argument('--player',type=Path,required=True);parser.add_argument('--output',type=Path);args=parser.parse_args()
    if args.output:result=run(args.player,args.output)
    else:
        with tempfile.TemporaryDirectory(prefix='azure-surface-uv-') as temporary:result=run(args.player,Path(temporary)/'result')
    print(json.dumps(dict(status='passed',captures=len(result))))
