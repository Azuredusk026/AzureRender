"""Create the exploration game with public assets or a local animated GLB."""
import argparse
import json
from pathlib import Path
import shutil
from create_third_person_project import create as courtyard
from test_independent_animation_gpu import component

SOURCE = Path(__file__).resolve().parents[1] / 'assets_public'

def create(root, primary=None):
    courtyard(root, primary)
    assets = root / 'assets'
    base = json.loads((assets / 'courtyard.azurelevel').read_text(encoding='utf-8'))
    (assets / 'courtyard.azurelevel').unlink()
    def write(path, value):
        path.write_text(json.dumps(value, ensure_ascii=False, indent=2)+'\n', encoding='utf-8')
    def node(name, position, scale=(1,1,1), extra=None, resource='cube'):
        data = {'azure.transform': component('azure.transform', {'translation': position, 'scale': list(scale)})}
        data.update(extra or {})
        return {'id': name, 'resourceId': resource, 'components': data}
    def box(name, position, scale, rotation=None):
        result=node(name,position,scale,{'azure.rigid-body':component('azure.rigid-body',{'halfExtent':[1,1,1]})})
        if rotation: result['components']['azure.transform']['data']['rotation']=rotation
        return result
    def script(name): return component('azure.script', {'asset':'assets:/'+name+'.lua'})
    prefabs = assets / 'prefabs'; prefabs.mkdir()
    def prefab(name, item, resource):
        item['id']='body'; item['resourceId']='mesh'
        write(prefabs/(name+'.azureprefab'), {'schemaVersion':1, 'id':name,
            'resources':[{'id':'mesh','asset':resource}], 'nodes':[item]})
    hero=base['nodes'][0]; hero['components'].update({'azure.task-state':component('azure.task-state',{}), 'azure.script':script('hero')})
    prefab('hero',hero,base['resources'][0]['asset'])
    guide=node('guide',[-1,0,-1],extra={'azure.animator':component('azure.animator',{'asset':'assets:/locomotion.json','startTime':.4},2),
        'azure.interactable':component('azure.interactable',{'prompt':'Talk to the guide [E]','range':2.5}), 'azure.script':script('guide')})
    prefab('guide',guide,'assets:/guide.gltf')
    collectible=node('artifact',[0,.45,0],(.3,.45,.3),{'azure.collectible':component('azure.collectible',{}),
        'azure.interactable':component('azure.interactable',{'prompt':'Collect artifact [E]','range':3,'offset':[0,.3,0]}), 'azure.script':script('collectible')})
    prefab('collectible',collectible,'assets:/cube.gltf')
    door=node('door',[0,1.5,-290],(2.5,1.5,.2),{'azure.rigid-body':component('azure.rigid-body',{'halfExtent':[1,1,1]}),
        'azure.door':component('azure.door',{}), 'azure.interactable':component('azure.interactable',{'prompt':'Open gate [E]','range':3.5,'offset':[0,0,0]}), 'azure.script':script('door')})
    prefab('door',door,'assets:/cube.gltf')
    checkpoint=node('checkpoint',[0,1,-165],(3,2,2),{'azure.rigid-body':component('azure.rigid-body',{'trigger':True,'halfExtent':[1,1,1]}),
        'azure.checkpoint':component('azure.checkpoint',{'position':[0,0,-165]}), 'azure.script':script('checkpoint')},'')
    prefab('checkpoint',checkpoint,'assets:/cube.gltf')
    # Invisible sensors retain transform and physics without visible geometry.
    cp=json.loads((prefabs/'checkpoint.azureprefab').read_text(encoding='utf-8'))
    cp['nodes'][0]['components']['azure.renderable']=component('azure.renderable',{'visible':False})
    write(prefabs/'checkpoint.azureprefab',cp)
    nodes=[box('ground',[0,-.25,-170],[23,.25,180]),
        box('passage-left',[-3.2,1.5,-96],[.2,1.5,12]),box('passage-right',[3.2,1.5,-96],[.2,1.5,12]),
        box('ramp',[0,.17,-170],[2,.15,2],[6,0,0]),
        box('interior-left',[-3.2,2,-305],[.2,2,18]),box('interior-right',[3.2,2,-305],[.2,2,18]),
        box('interior-roof',[0,2.35,-305],[3.4,.15,18]),
        box('gate-left',[-13,1.5,-290],[10.5,1.5,.2]),box('gate-right',[13,1.5,-290],[10.5,1.5,.2]),
        node('camera',[0,0,0],extra={'azure.third-person-camera':component('azure.third-person-camera',{'target':'hero:body','distance':5.5,'targetHeight':1,'shoulder':.2})},resource=''),
        node('hud',[0,0,0],extra={'azure.game-ui':component('azure.game-ui',{'asset':'assets:/hud.rml'})},resource=''),
        node('goal',[0,1,-330],(3,2,3),{'azure.rigid-body':component('azure.rigid-body',{'trigger':True,'halfExtent':[1,1,1]}),'azure.script':script('goal')},resource='')]
    for i,p in enumerate(([-4,0,-32],[4,0,-185])):
        nodes.append(node('companion-'+str(i),p,extra={'azure.animator':component('azure.animator',{'asset':'assets:/locomotion.json','startTime':.7+i},2)},resource='guide'))
    instances=[{'instance':name,'asset':'assets:/prefabs/'+name+'.azureprefab'} for name in ('hero','guide','door','checkpoint')]
    for i,p in enumerate(([-14,.45,-50],[14,.45,-130],[-14,.45,-210])):
        instances.append({'instance':'artifact-'+str(i),'asset':'assets:/prefabs/collectible.azureprefab',
            'overrides':{'body':{'components':{'azure.transform':{'data':{'translation':p}}}}}})
    level={'schemaVersion':1,'id':'exploration','sceneType':'character','renderSettings':{'platform':False},
        'resources':base['resources'][1:],'nodes':nodes,'prefabs':instances}
    write(assets/'exploration.azurelevel',level)
    for path in (SOURCE/'playable_rules').iterdir(): shutil.copy2(path,assets/path.name)
    shutil.copy2(SOURCE/'third_person/CHARACTER-LICENSE.txt',assets/'CHARACTER-LICENSE.txt')
    project=json.loads((root/'project.azureproject').read_text(encoding='utf-8'))
    project.update(name='Azure Exploration',startupScene='assets:/exploration.azurelevel')
    write(root/'project.azureproject',project)

if __name__=='__main__':
    parser=argparse.ArgumentParser(__doc__);parser.add_argument('--output',type=Path,required=True);parser.add_argument('--character',type=Path)
    args=parser.parse_args();create(args.output,args.character)
