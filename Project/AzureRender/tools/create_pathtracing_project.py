"""Create an independent, editable path tracing demonstration using CC0 geometry."""
import argparse
import json
from pathlib import Path
import uuid


def create(root):
    root = Path(root)
    if root.exists() and any(root.iterdir()):
        raise ValueError('Demo output directory must be empty')
    assets = root / 'assets'
    assets.mkdir(parents=True)
    source = Path(__file__).resolve().parents[1] / 'assets_public/scene_inspector/assets/cube.gltf'
    cube = json.loads(source.read_text(encoding='utf-8'))
    def write(path, value):
        path.write_text(json.dumps(value, ensure_ascii=False, indent=2) + '\n', encoding='utf-8')
    resources = []
    for name, color, metal, rough in [('white',[.75,.75,.75,1],0,.8),('red',[.65,.035,.025,1],0,.65),('blue',[.03,.12,.65,1],0,.65),('metal',[.9,.7,.35,1],1,.18)]:
        model = json.loads(json.dumps(cube))
        model['materials'] = [{'name':name,'doubleSided':True,'pbrMetallicRoughness':{'baseColorFactor':color,'metallicFactor':metal,'roughnessFactor':rough}}]
        for mesh in model['meshes']:
            for primitive in mesh['primitives']:
                primitive['material'] = 0
        write(assets / (name+'.gltf'), model)
        resources.append({'id':name,'asset':'assets:/'+name+'.gltf'})
    def component(kind,data): return {'type':kind,'version':1,'data':data}
    def box(name, material, position, scale):
        return {'id':name,'resourceId':material,'components':{'azure.transform':component('azure.transform',{'translation':position,'scale':scale})}}
    nodes = [box('floor','white',[0,-.1,0],[3,.1,3]),box('back','white',[0,1.5,-2],[3,1.5,.1]),box('left','red',[-2,1.5,0],[.1,1.5,2]),box('right','blue',[2,1.5,0],[.1,1.5,2]),box('diffuse-box','white',[-.7,.5,0],[.45,.5,.45]),box('metal-box','metal',[.7,.75,-.5],[.45,.75,.45])]
    nodes.append({'id':'key-light','components':{'azure.transform':component('azure.transform',{'translation':[0,2.7,.5]})}})
    nodes.append({'id':'camera','components':{'azure.transform':component('azure.transform',{'translation':[0,1.2,5]}),'azure.camera':component('azure.camera',{'enabled':True,'priority':1,'fieldOfView':55})}})
    settings={'renderer':'pathtracing','pathTraceSamples':256,'pathTraceBounces':6,'skyMode':2,'skyColor':[.02,.025,.035],'platform':False,'outline':0}
    write(assets/'room.azurelevel',{'schemaVersion':1,'id':'room','sceneType':'pathtracing','renderSettings':settings,'resources':resources,'nodes':nodes,'lights':[{'id':'key','nodeId':'key-light','color':[1,.9,.75],'intensity':35,'radius':10}]})
    write(root/'project.azureproject',{'schemaVersion':1,'id':str(uuid.uuid4()),'name':'Path Tracing Room','engine':{'id':'AzureRender','projectApiVersion':1},'mounts':[{'name':'assets','path':'assets'}],'startupScene':'assets:/room.azurelevel'})
    write(root/'SOURCES.json',{'schemaVersion':1,'geometry':{'source':'AzureRender public smoke-test cube','license':'CC0-1.0'},'scene':'Original AzureRender demonstration','reference':{'repository':'gkNextEngine','commit':'4ba5b7cd106c282e7ed166ff853aeea87b392680','scope':'Progressive sampling and invalidation concepts; no third-party rendering code or playground assets copied'}})
    return root/'project.azureproject'

if __name__ == '__main__':
    parser=argparse.ArgumentParser(__doc__)
    parser.add_argument('--output',type=Path,required=True)
    print(create(parser.parse_args().output))
