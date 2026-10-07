"""Create a world-space 3C project using public or local character assets."""
import argparse
import json
from pathlib import Path
import shutil
from test_independent_animation_gpu import component

SOURCE=Path(__file__).resolve().parents[1]/'assets_public'
def create(root,primary=None):
    if root.exists() and any(root.iterdir()):raise FileExistsError(root)
    assets=root/'assets';assets.mkdir(parents=True,exist_ok=True)
    primary=primary or SOURCE/'third_person/explorer.gltf';private=primary.suffix=='.glb'
    primary_name='hero'+primary.suffix
    shutil.copy2(primary,assets/primary_name);shutil.copy2(SOURCE/'third_person/guide.gltf',assets/'guide.gltf')
    cube=json.loads((SOURCE/'test_model.gltf').read_text(encoding='utf-8'))
    cube['nodes']=[{'mesh':0,'name':'unit-cube'}];cube['scenes']=[{'nodes':[0]}];cube.pop('skins',None);cube.pop('animations',None)
    cube['materials']=[{'name':'stone','pbrMetallicRoughness':{'baseColorFactor':[.36,.42,.48,1],'metallicFactor':0,'roughnessFactor':.9}}]
    cube['asset']['extras']={'license':'CC0-1.0'}
    for primitive in cube['meshes'][0]['primitives']:
        primitive['material']=0;primitive.pop('targets',None)
        for attribute in ('JOINTS_0','WEIGHTS_0'):primitive['attributes'].pop(attribute,None)
    (assets/'cube.gltf').write_text(json.dumps(cube,indent=2)+'\n',encoding='utf-8')
    def box(name,p,size,rotation=None):
        return {'id':name,'resourceId':'cube','components':{
            'azure.transform':component('azure.transform',{'translation':p,'scale':size,'rotation':rotation or [0,0,0]}),
            'azure.rigid-body':component('azure.rigid-body',{'halfExtent':[1,1,1]})}}
    nodes=[{'id':'hero','resourceId':'hero','components':{
        'azure.transform':component('azure.transform',{'translation':[0,.34 if private else 0,0]}),
        'azure.character':component('azure.character',{'speed':2,'sprintMultiplier':2.5,'forwardYaw':0 if private else 180,
            'centerOffset':.56 if private else .9},3),
        'azure.animator':component('azure.animator',{'asset':'assets:/locomotion.json','locomotion':True,'referenceSpeed':2},2)}},
        {'id':'guide','resourceId':'guide','components':{
            'azure.transform':component('azure.transform',{'translation':[-3,0,-4]}),
            'azure.animator':component('azure.animator',{'asset':'assets:/locomotion.json','startTime':.4},2)}},
        {'id':'camera','components':{'azure.third-person-camera':component('azure.third-person-camera',
            {'target':'hero','distance':5.5,'targetHeight':1.0,'shoulder':.2})}},
        box('ground',[0,-.25,-3],[10,.25,10]),
        box('step',[0,.1,-2],[.75,.1,.3]),
        box('wall',[0,1,-5.2],[3,1,.15]),
        box('side-wall',[3.5,1,-3],[.15,1,2.2]),
        box('ramp',[-4,.35,-2],[.8,.15,1.8],[12,0,0])]
    level={'schemaVersion':1,'id':'third-person-courtyard','sceneType':'character','renderSettings':{'platform':False},
        'resources':[{'id':'hero','asset':'assets:/'+primary_name},{'id':'guide','asset':'assets:/guide.gltf'},
            {'id':'cube','asset':'assets:/cube.gltf'}],'nodes':nodes}
    (assets/'courtyard.azurelevel').write_text(json.dumps(level,ensure_ascii=False,indent=2)+'\n',encoding='utf-8')
    (assets/'locomotion.json').write_text(json.dumps({'schemaVersion':1,'initial':'idle','states':[
        {'name':'idle','clip':0},{'name':'walk','clip':1}],'transitions':[]},indent=2)+'\n',encoding='utf-8')
    (root/'project.azureproject').write_text(json.dumps({'schemaVersion':1,'id':'7f971748-131f-4764-9aac-221848014a37',
        'name':'第三人称庭院','mounts':[{'name':'assets','path':'assets'}],'startupScene':'assets:/courtyard.azurelevel'},ensure_ascii=False,indent=2)+'\n',encoding='utf-8')

if __name__=='__main__':
    parser=argparse.ArgumentParser(__doc__);parser.add_argument('--output',type=Path,required=True);parser.add_argument('--character',type=Path)
    args=parser.parse_args();create(args.output,args.character)
