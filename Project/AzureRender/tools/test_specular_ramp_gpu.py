"""Validate specular-ramp ownership and source arithmetic through production draws."""
import argparse,base64,hashlib,json,re,struct,subprocess,tempfile,uuid
from pathlib import Path
import numpy as np
from PIL import Image
from test_character_probe_gpu import prepare as probe_project,linear,PROBE_RGB
from convert_material_semantics import complete_material_document

NORMAL=np.array([.22,.07,1.]);NORMAL/=np.linalg.norm(NORMAL)
PIXEL=(64,64)
BUDGET=.006
CASES=['ramp','fixed','vertex','flip','iridescent','half','floor-zero','floor-one','no-main','no-main-iridescent','omitted','omitted-fixed','document','generic','illumination']

def sample(image,uv):
    height,width=image.shape[:2];x,y=np.asarray(uv)*[width,height]-.5;ix,iy=np.floor([x,y]).astype(int);fx,fy=x-ix,y-iy
    return sum(image[np.clip(iy+dy,0,height-1),np.clip(ix+dx,0,width-1)]*wx*wy for dx,wx in [(0,1-fx),(1,fx)] for dy,wy in [(0,1-fy),(1,fy)])

def prepare(root,name):
    project=probe_project(root,dict(name=name,material='fabric'));assets=root/'assets';path=assets/'surface.gltf';model=json.loads(path.read_text());payload=bytearray(base64.b64decode(model['buffers'][0]['uri'].split(',',1)[1]));payload[48:96]=struct.pack('<12f',*(NORMAL.tolist()*4));model['buffers'][0]['uri']='data:application/octet-stream;base64,'+base64.b64encode(payload).decode()
    base=np.array([120,150,180,255],dtype=np.uint8);orm=np.array([102,89,128,204],dtype=np.uint8)
    diffuse=np.array([[[255,255,255,a] for a in [51,89,153,230]]],dtype=np.uint8)
    ramp=np.zeros((16,16,4),dtype=np.uint8)
    for y in range(16):
        for x in range(16):ramp[y,x]=[40+x*12,60+y*9,220-x*7,255]
    neutral=np.array([[[127,127,255,255],[128,128,255,255]],[[128,128,255,255],[127,127,255,255]]],dtype=np.uint8)
    images=[np.tile(base,(2,2,1)),np.tile(orm,(2,2,1)),np.full((2,4,4),255,dtype=np.uint8),diffuse,ramp,neutral];files=['base','orm','lut','diffuse','specular','normal']
    for file,image in zip(files,images):Image.fromarray(image).save(assets/(file+'.png'))
    model['images']=[dict(uri=file+'.png') for file in files];model['textures']=[dict(source=i) for i in range(6)];material=model['materials'][0];material['pbrMetallicRoughness'].update(baseColorFactor=[1,1,1,1],baseColorTexture=dict(index=0),metallicRoughnessTexture=dict(index=1),metallicFactor=1,roughnessFactor=1);material['occlusionTexture']=dict(index=1);material['normalTexture']=dict(index=5)
    iridescence=1 if name in ['iridescent','no-main-iridescent'] else .5 if name=='half' else 0;floor=0 if name=='floor-zero' else 1 if name=='floor-one' else .55
    profile=dict(schemaVersion=2,**{'class':'generic' if name=='generic' else 'fabric'},features=['diffuse-ramp'],styleParameters=[0,1,1,0],shadowColorLut=dict(schemaVersion=1,texture=2,flipV=False),diffuseRamp=dict(schemaVersion=1,texture=3),specularInput=dict(schemaVersion=1,source='metallic-roughness-alpha',scale=.5),nprDiffuse=dict(schemaVersion=1,shadowScale=1,lightingInputs='host-scene-radiance',shadowMask='none',baseAo='metallic-roughness-red',mainRamp=dict(schemaVersion=1,bias=0)),specularRamp=dict(schemaVersion=1,texture=4,flipV=name=='flip',iridescence=iridescence,lightingInputs='host-main-light'))
    if name.startswith('omitted'):profile.pop('specularRamp')
    if name=='generic':
        profile['nprDiffuse']['shadowAlbedo']='base-color';profile.pop('shadowColorLut')
    material['extras']=dict(azureSlotId='surface',azureRenderMaterial=profile);path.write_text(json.dumps(model));level_path=assets/'scene.azurelevel';level=json.loads(level_path.read_text());level['nodes'][1]['components']['azure.camera']['data'].update(orthographic=True,orthographicHeight=1);level['nodes'].append(dict(id='main'));level['lights']=[] if name.startswith('no-main') else [dict(id='main',nodeId='main',intensity=1.5,radius=10,shape=dict(type='directional',direction=[0,0,-1]))];level['renderSettings'].update(nprLighting=dict(schemaVersion=1,enabled=True,specularFloor=floor,ambientWeight=1 if name=='illumination' else 0,ambientColor=[.25,.5,1]),environmentSpecularModel='legacy-schlick');level['reflectionProbe']['intensity']=.1
    if name=='document':
        ids={str(i):str(uuid.uuid4()) for i in range(6)}
        for i,file in enumerate(files):(assets/(file+'.png.azmeta')).write_text(json.dumps(dict(schemaVersion=1,id=ids[str(i)],settings={},dependencies=[])))
        document_id=str(uuid.uuid4());(assets/'surface.azurematerial').write_text(json.dumps(complete_material_document(material,model,ids)));(assets/'surface.azurematerial.azmeta').write_text(json.dumps(dict(schemaVersion=1,id=document_id,settings={},dependencies=list(ids.values()))));level['nodes'][0]['materialSlots']={'surface':document_id}
    level_path.write_text(json.dumps(level));return project,dict(name=name,base=base,orm=orm,diffuse=diffuse,ramp=ramp,iridescence=iridescence,floor=floor)

def reference(data,isolation):
    view=np.array([-1/256,1/256,3.]);view/=np.linalg.norm(view);normal=NORMAL;light=np.array([0.,0.,1.]);camera=np.array([0.,0.,1.]);rough=float(data['orm'][1])/255;metal=float(data['orm'][2])/255;nv=np.clip(np.dot(normal,view),0,1);q=np.array([camera[0],light[1],camera[2]]);q/=np.linalg.norm(q);half=light+2*q+3*view;half/=np.linalg.norm(half);nh=np.dot(normal,half);a=max(rough*rough,1/128);a2=a*a;D=a2/(1+(a2-1)*nh*nh)**2;lobe=np.clip(.5*D/(2*nv+a+.0001)-2**-14,0,20);uv=[(1-data['iridescence'])*D*(a2+.0001)+data['iridescence']*nv*nv,rough*(1-metal)]
    if data['name']=='flip':uv[1]=1-uv[1]
    ramp=sample(data['ramp'].astype(float)/255,uv)[:3];base=linear(data['base'][:3]);f0=.04*(float(data['orm'][3])/255)*(1-metal)+base*metal
    if isolation=='reflection':
        if not data['name'].startswith('omitted'):f0=f0*(1-data['iridescence'])+f0*ramp*data['iridescence']
        return PROBE_RGB*.1*(f0+(1-f0)*(1-nv)**5)
    if data['name'].startswith('no-main'):return np.zeros(3)
    if data['name'].startswith('omitted'):return None
    view_alpha=sample(data['diffuse'].astype(float)/255,[(np.dot(normal,camera)+1)*.5,.5])[3];main_alpha=sample(data['diffuse'].astype(float)/255,[(np.dot(normal,light)+1)*.5,.5])[3];ao=float(data['orm'][0])/255;full=min(ao,main_alpha,1)
    illumination=np.array([1.75,2,2.5]) if data['name']=='illumination' else np.full(3,1.5)
    return f0*ramp*lobe*illumination*(.5+.5*full)*(data['floor']+(1-data['floor'])*full)

def run(player,output,validation=None,resources=None):
    output.mkdir(parents=True,exist_ok=False);rows=[];pictures={}
    for isolation,names in [('direct-specular',CASES),('reflection',CASES),('direct-diffuse',['ramp','flip','iridescent','omitted']),('emissive',['ramp','flip','iridescent','omitted'])]:
        for name in names:
            key=isolation+'-'+name;root=output/key;project,data=prepare(root,name);command=[str(player.resolve()),'--project',str(project.resolve()),'--width','128','--height','128','--capture-dir',str(root/'frames'),'--capture-frames','1','--fixed-frame-step','--qa-camera','scene','--qa-light','scene','--qa-isolation',isolation,'--runtime-report',str(root/'runtime.json')]
            if name in ['fixed','omitted-fixed']:command+=['--disable-bindless','--disable-parallel-recording']
            if name=='vertex':command+=['--disable-compute-skinning']
            if resources:command+=['--resource-root',str(resources.resolve())]
            (root/'command.json').write_text(json.dumps(command,indent=2));print(key,flush=True)
            with (root/'player.log').open('w') as log:subprocess.run(command,stdout=log,stderr=subprocess.STDOUT,check=True,timeout=180)
            text=(root/'player.log').read_text(errors='replace');assert not re.search(r'VUID-|Validation (?:Error|Warning)|\[error\]',text);assert 'Allocator after unload: buffers=0 images=0' in text
            if validation:assert ('Validation layer: enabled' in text)==(validation=='enabled')
            runtime=json.loads((root/'runtime.json').read_text());assert not runtime['scriptErrors'] and not runtime['presentationErrors'];frame=root/'frames/frame_000000.png';image=np.asarray(Image.open(frame).convert('RGB'));pictures[key]=image;wanted=reference(data,isolation) if isolation in ['direct-specular','reflection'] else None;error=None if wanted is None else float(np.max(abs(linear(image[64,64])-wanted)));row=dict(case=key,pixel=image[64,64].tolist(),expected=None if wanted is None else wanted.tolist(),maximumLinearError=error,imageSha256=hashlib.sha256(frame.read_bytes()).hexdigest());rows.append(row);(output/'summary.json').write_text(json.dumps(dict(status='running',linearErrorBudget=BUDGET,captures=rows),indent=2));assert error is None or error<=BUDGET,row
    for isolation in ['direct-specular','reflection']:
        for name in ['fixed','document']:assert np.array_equal(pictures[isolation+'-ramp'],pictures[isolation+'-'+name]),(isolation,name)
        assert np.array_equal(pictures[isolation+'-omitted'],pictures[isolation+'-omitted-fixed'])
    for isolation in ['direct-diffuse','emissive']:
        for name in ['flip','iridescent','omitted']:assert np.array_equal(pictures[isolation+'-ramp'],pictures[isolation+'-'+name]),(isolation,name)
    for name in ['flip','floor-zero','floor-one','illumination']:assert np.array_equal(pictures['reflection-ramp'],pictures['reflection-'+name])
    assert np.max(abs(pictures['reflection-ramp'].astype(int)-pictures['reflection-iridescent'].astype(int)))>2
    (output/'summary.json').write_text(json.dumps(dict(status='passed',linearErrorBudget=BUDGET,executableSha256=hashlib.sha256(player.read_bytes()).hexdigest(),captures=rows),indent=2))

if __name__=='__main__':
    parser=argparse.ArgumentParser(description=__doc__);parser.add_argument('--executable',type=Path,required=True);parser.add_argument('--output',type=Path);parser.add_argument('--expected-validation',choices=['enabled','disabled']);parser.add_argument('--resource-root',type=Path);args=parser.parse_args()
    if args.output:run(args.executable,args.output.resolve(),args.expected_validation,args.resource_root)
    else:
        with tempfile.TemporaryDirectory(prefix='azure-specular-ramp-') as folder:run(args.executable,Path(folder)/'captures',args.expected_validation)
