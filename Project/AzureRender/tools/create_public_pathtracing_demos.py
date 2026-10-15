"""Assemble independent public-model projects from the locked source cache."""
import argparse,json,math,shutil,uuid,hashlib
from pathlib import Path
import numpy as np
from PIL import Image
from create_pathtracing_project import create
MODELS=('DamagedHelmet','ABeautifulGame','TransmissionRoughnessTest')
def bounds(d):
    def local(n):
        if 'matrix' in n:return np.array(n['matrix']).reshape(4,4).T
        x,y,z,w=n.get('rotation',[0,0,0,1]);m=np.eye(4)
        m[:3,:3]=np.array([[1-2*y*y-2*z*z,2*x*y-2*z*w,2*x*z+2*y*w],[2*x*y+2*z*w,1-2*x*x-2*z*z,2*y*z-2*x*w],[2*x*z-2*y*w,2*y*z+2*x*w,1-2*x*x-2*y*y]])@np.diag(n.get('scale',[1,1,1]));m[:3,3]=n.get('translation',[0,0,0]);return m
    nodes=d['nodes'];parents={c:i for i,n in enumerate(nodes) for c in n.get('children',[])};cache={}
    def world(i):
        if i not in cache:cache[i]=(world(parents[i]) if i in parents else np.eye(4))@local(nodes[i])
        return cache[i]
    points=[]
    for i,n in enumerate(nodes):
        if 'mesh' not in n:continue
        for p in d['meshes'][n['mesh']]['primitives']:
            a=d['accessors'][p['attributes']['POSITION']];lo=a['min'];hi=a['max']
            for x in [lo[0],hi[0]]:
                for y in [lo[1],hi[1]]:
                    for z in [lo[2],hi[2]]:points.append((world(i)@np.array([x,y,z,1]))[:3])
    return np.min(points,axis=0),np.max(points,axis=0)
def build(sources,output):
    sources=Path(sources);output=Path(output);results=[]
    lock=json.loads((sources/'SOURCES.json').read_text(encoding='utf-8'))
    for name in MODELS:
        root=output/name
        if root.exists():raise ValueError('Project directory already exists: '+str(root))
        project=create(root);assets=root/'assets';model=assets/'model';shutil.copytree(sources/'Models'/name/'glTF',model)
        original=next(model.glob('*.gltf'));d=json.loads(original.read_text(encoding='utf-8'));lo,hi=bounds(d)
        size=hi-lo;scale=3.6/max(size);center=(lo+hi)*.5;position=[-center[0]*scale,.15-lo[1]*scale,-center[2]*scale]
        transforms=[];cap=512 if name=='ABeautifulGame' else 1024
        for file in model.iterdir():
            if file.suffix.lower() not in ('.jpg','.png'):continue
            image=Image.open(file)
            if max(image.size)>cap:
                before=image.size;image.thumbnail((cap,cap),Image.Resampling.LANCZOS);image.save(file,quality=95)
                transforms.append({'file':file.name,'originalSize':before,'size':image.size,'filter':'Lanczos'})
        if name=='ABeautifulGame':
            from simplify_gltf_demo import simplify
            transforms.append(simplify(original,.6))
        level=json.loads((assets/'room.azurelevel').read_text(encoding='utf-8'))
        def component(kind,data):return {'type':kind,'version':1,'data':data}
        def node(id,res,pos,sc=(1,1,1)):
            return {'id':id,'resourceId':res,'components':{'azure.transform':component('azure.transform',{'translation':list(pos),'scale':list(sc)})}}
        # Original emitter geometry is part of the demo project.
        cube=json.loads((assets/'white.gltf').read_text(encoding='utf-8'));cube['materials'][0]['emissiveFactor']=[9,8.5,8]
        (assets/'emitter.gltf').write_text(json.dumps(cube),encoding='utf-8')
        level['resources']=[{'id':'subject','asset':'assets:/model/'+original.name},{'id':'floor','asset':'assets:/white.gltf'},{'id':'emitter','asset':'assets:/emitter.gltf'}]
        subject=node('subject','subject',position,[scale]*3);floor=node('floor','floor',[0,-.06,0],[6,.06,6]);emitter=node('softbox','emitter',[-1,4,1],[1.4,.025,.8])
        height=float(size[1]*scale);targetY=.15+height*.45
        cameraPos=[0,targetY+3,4.8] if name=='ABeautifulGame' else [0,targetY,6]
        pitch=math.degrees(math.atan2(cameraPos[1]-targetY,cameraPos[2]))
        camera=node('camera','',cameraPos);camera['components']['azure.transform']['data']['rotation']=[pitch,0,0]
        camera['components']['azure.camera']=component('azure.camera',{'enabled':True,'priority':1,'fieldOfView':42})
        level['nodes']=[subject,floor,emitter,camera,node('key','',[2,3,3]),node('fill','',[-3,2,2])]
        level['lights']=[{'id':'key','nodeId':'key','color':[1,.94,.85],'intensity':50,'radius':10},{'id':'fill','nodeId':'fill','color':[.65,.8,1],'intensity':10,'radius':10}]
        level['renderSettings'].update(pathTraceSamples=256,pathTraceBounces=10,pathTraceDenoise=True,skyColor=[.15,.18,.25],bloomEnabled=False,stylized=False)
        (assets/'room.azurelevel').write_text(json.dumps(level,indent=2),encoding='utf-8')
        p=json.loads(project.read_text(encoding='utf-8'));p['name']=name+' Ray Tracing';project.write_text(json.dumps(p,indent=2),encoding='utf-8')
        for filename in ('README.md','LICENSE.md','metadata.json'):shutil.copy2(sources/'Models'/name/filename,root/filename)
        source={'schemaVersion':1,'repository':lock['repository'],'commit':lock['commit'],'model':name,'license':'CC-BY-4.0','sourceFiles':[x for x in lock['files'] if '/'+name+'/' in x['path']],'processing':transforms,'placement':{'scale':scale,'position':position},'scene':'AzureRender original studio lighting and layout'}
        (root/'SOURCES.json').write_text(json.dumps(source,indent=2),encoding='utf-8')
        results.append({'model':name,'project':str(project),'height':height,'camera':cameraPos,'pitch':pitch,'textureLimit':cap})
    print(json.dumps(results,indent=2));return results
if __name__=='__main__':
    p=argparse.ArgumentParser(__doc__);p.add_argument('--sources',type=Path,required=True);p.add_argument('--output',type=Path,required=True);a=p.parse_args();build(a.sources,a.output)
