"""Author the exploration route and deterministic Blender environment."""
import argparse,hashlib,json,math,random,shutil,subprocess,tempfile
from pathlib import Path
from PIL import Image
from create_playable_project import create
from test_independent_animation_gpu import component
from audit_face_sdf_compatibility import load_gltf

SOURCE=Path(__file__).resolve().parents[1]
def write(path,value):path.write_text(json.dumps(value,ensure_ascii=False,indent=2)+'\n',encoding='utf-8')
def textures(folder):
    folder.mkdir(parents=True,exist_ok=True);n=256
    for name,color in [('stone',(116,123,110)),('sand',(157,133,87)),('leaf',(54,86,39)),('relic',(218,165,45))]:
        rng=random.Random('azure-environment/1/'+name)
        height=[[.5+.13*math.sin(x*.19+y*.07)+.12*math.sin(y*.27-x*.045)+rng.uniform(-.08,.08) for x in range(n)] for y in range(n)]
        albedo=Image.new('RGB',(n,n));normal=Image.new('RGB',(n,n));orm=Image.new('RGB',(n,n))
        for y in range(n):
            for x in range(n):
                h=height[y][x];gain=.78+.42*h
                albedo.putpixel((x,y),tuple(max(0,min(255,round(c*gain))) for c in color))
                dx=height[y][(x+1)%n]-height[y][(x-1)%n];dy=height[(y+1)%n][x]-height[(y-1)%n][x]
                v=(-dx,-dy,1);length=math.sqrt(sum(k*k for k in v))
                normal.putpixel((x,y),tuple(round((k/length*.5+.5)*255) for k in v))
                orm.putpixel((x,y),(255,round((80+35*h) if name=='relic' else (180+55*h)),220 if name=='relic' else 0))
        for suffix,image in [('albedo',albedo),('normal',normal),('orm',orm)]:image.save(folder/f'{name}-{suffix}.png')

def author(output,blender,character=None):
    if character:
        clips=load_gltf(character).get('animations',[])
        if len(clips)<2 or [c.get('name','').lower() for c in clips[:2]]!=['idle','walk']:
            raise ValueError('Exploration character requires idle and walk as its first two animations')
    output=output.resolve();output.mkdir(parents=True,exist_ok=True)
    with tempfile.TemporaryDirectory(prefix='azure-exploration-generator-') as temporary:
        candidate=Path(temporary)/'game';create(candidate,character)
        assets=candidate/'assets';environment=assets/'environment';textures(environment)
        result=subprocess.run([str(blender),'-b','--factory-startup','-t','2','--python',str(SOURCE/'tools/blender_exploration_environment.py'),'--',str(environment)],capture_output=True,text=True,encoding='utf-8',errors='replace',timeout=180)
        (output/'BLENDER-GENERATION.log').write_text(result.stdout+result.stderr,encoding='utf-8')
        if result.returncode:raise RuntimeError('Blender environment generation failed; see BLENDER-GENERATION.log')
        level=json.loads((assets/'exploration.azurelevel').read_text(encoding='utf-8'));nodes={n['id']:n for n in level['nodes']}
        def transform(id,position,scale=None):
            data=nodes[id]['components']['azure.transform']['data'];data['translation']=position
            if scale:data['scale']=scale
        transform('ground',[0,-.25,-55],[1,1,1]);nodes['ground']['resourceId']='ground'
        nodes['ground']['components']['azure.rigid-body']['data']['halfExtent']=[12,.25,58]
        transform('passage-left',[-2.5,1.5,-36],[.2,1.5,5]);transform('passage-right',[2.5,1.5,-36],[.2,1.5,5])
        transform('ramp',[0,.17,-61],[2,.15,2])
        for id,x in [('interior-left',-3.2),('interior-right',3.2)]:transform(id,[x,2,-103],[.2,2,10])
        transform('interior-roof',[0,2.35,-103],[3.4,.15,10])
        for id,x in [('gate-left',-8),('gate-right',8)]:transform(id,[x,1.5,-94],[5.5,1.5,.2])
        transform('goal',[0,1,-108],[3,2,3]);transform('companion-0',[-4,0,-14]);transform('companion-1',[4,0,-65])
        def prefab_position(name,position):
            path=assets/f'prefabs/{name}.azureprefab';data=json.loads(path.read_text(encoding='utf-8'));data['nodes'][0]['components']['azure.transform']['data']['translation']=position
            if name=='checkpoint':data['nodes'][0]['components']['azure.checkpoint']['data']['position']=[0,0,-58]
            write(path,data)
        prefab_position('door',[0,1.5,-94]);prefab_position('checkpoint',[0,1,-58])
        markers=[[-5,.45,-18],[5,.45,-45],[-5,.45,-74]]
        for instance,position in zip([i for i in level['prefabs'] if i['instance'].startswith('artifact-')],markers):instance['overrides']['body']['components']['azure.transform']['data']['translation']=position
        relic=assets/'prefabs/collectible.azureprefab';data=json.loads(relic.read_text(encoding='utf-8'));data['resources'][0]['asset']='assets:/environment/relic.glb';write(relic,data)
        environment_assets=[f'assets:/environment/{name}.glb' for name in ['ground','trail','branch','rock','shrub','ruin','arch','relic']]
        level['resources'] += [{'id':Path(ref).stem,'asset':ref} for ref in environment_assets]
        def node(id,resource,position,scale=(1,1,1),rotation=(0,0,0)):
            level['nodes'].append({'id':id,'resourceId':resource,'components':{'azure.transform':component('azure.transform',{'translation':position,'scale':list(scale),'rotation':list(rotation)})}})
        node('main-trail','trail',[0,.012,-55]);node('gate-arch','arch',[0,0,-94])
        level['nodes'][-2]['components']['azure.rigid-body']=component('azure.rigid-body',{'halfExtent':[2,.01,55]})
        for index,p in enumerate(markers):
            node('side-trail-'+str(index),'branch',[p[0]/2,.014,p[2]])
            level['nodes'][-1]['components']['azure.rigid-body']=component('azure.rigid-body',{'halfExtent':[3,.01,1]})
            node('artifact-landmark-'+str(index),'ruin',[p[0]+(-1.8 if p[0]<0 else 1.8),0,p[2]-1],(.7,.7,.7))
        rng=random.Random(260910)
        for i in range(18):
            z=-6-i*5.7;x=(-1 if i%2 else 1)*rng.uniform(7.3,10.5);s=rng.uniform(.7,1.3)
            node('near-rock-'+str(i),'rock',[x,0,z],[s,s,s],(0,rng.uniform(0,360),0))
            node('near-shrub-'+str(i),'shrub',[x+(-1 if x<0 else 1),0,z+1.8],[s,s,s])
        for i in range(8):node('distant-ridge-'+str(i),'rock',[(-1 if i%2 else 1)*rng.uniform(27,43),-1,-12-i*15],(rng.uniform(7,11),rng.uniform(5,8),rng.uniform(7,11)))
        for i in range(6):node('ruin-fragment-'+str(i),'ruin',[(-1 if i%2 else 1)*9,0,-23-i*11],(.7,.7,.7),(0,i*41,(-12 if i%2 else 8)))
        level['lights']=[]
        for index,p in enumerate(markers+[[0,2,-94]]):
            id='route-light-'+str(index);node(id,'',[p[0],p[1]+2,p[2]])
            level['lights'].append({'id':id,'nodeId':id,'color':[1,.76,.35] if index<3 else [.3,.75,1],'intensity':3,'radius':6})
        level['renderSettings'].update(cameraFar=300,shadowDistance=80);write(assets/'exploration.azurelevel',level)
        (assets/'hero.lua').write_text('''function update(dt)
    if self:pressed('restart') then self:load_level('assets:/exploration.azurelevel') end
    local collected=self:get('azure.task-state','collected')
    if self:get('azure.task-state','completed') then self:ui_text('objective','Expedition complete! Press R to explore again.')
    elseif self:get('azure.task-state','doorOpened') then self:ui_text('objective','Enter the illuminated hall.')
    elseif self:get('azure.task-state','started') then self:ui_text('objective','Find three artifacts by the golden stone markers.')
    else self:ui_text('objective','Talk to the guide at the trailhead [E].') end
    self:ui_text('progress','Artifacts: '..collected..' / 3')
    local id=self:interaction_target();local target=id~='' and self:find(id) or nil
    self:ui_text('prompt',target and target:get('azure.interactable','prompt') or '')
    local checkpoint=self:find('checkpoint:body')
    self:ui_text('checkpoint',checkpoint and checkpoint:get('azure.checkpoint','activated') and 'Checkpoint reached' or '')
    self:ui_text('route','Follow the sand trail and golden landmarks to the blue gate.')
end
''',encoding='utf-8')
        waypoints=[[0,0],[0,-18],[-5,-18],[0,-18],[0,-45],[5,-45],[0,-45],[0,-74],[-5,-74],[0,-74],[0,-93],[0,-108]]
        meters=sum(math.dist(a,b) for a,b in zip(waypoints,waypoints[1:]))
        write(candidate/'EXPLORATION-DESIGN.json',dict(schemaVersion=1,routeMeters=meters,passageWidth=4.6,walkRoute=waypoints,interactionStops=[0,2,5,8,10],environmentAssets=environment_assets,walkSpeed=2,sprintSpeed=5,learningRamp=True))
        write(candidate/'ENVIRONMENT-GENERATION.json',dict(schemaVersion=1,generator='azure.exploration-environment',version=1,seed=260910,license='CC0-1.0',blender=(environment/'blender-version.txt').read_text().strip(),inputs={name:hashlib.sha256((SOURCE/'tools'/name).read_bytes()).hexdigest() for name in ['generate_exploration_environment.py','blender_exploration_environment.py']},outputs={p.relative_to(candidate).as_posix():hashlib.sha256(p.read_bytes()).hexdigest() for p in sorted(environment.iterdir()) if p.is_file()},textureSemantics=dict(albedo='sRGB',normal='linear tangent-space +Y',roughness='linear green',metallic='linear blue')))
        (candidate/'GAME-GUIDE.md').write_text('''# 探索关卡操作说明

使用 WASD 移动，Shift 冲刺，空格跳跃。
鼠标旋转相机，滚轮调整距离。
E 触发当前提示的交互，R 重新开始。
Esc 释放光标，点击游戏区域恢复控制。

先与起点的引导角色交谈。
沿沙土道路寻找金色石柱旁的三个物件。
检查点位于道路中段，坡道用于练习地形移动。
收集完成后，打开蓝色灯光标记的大门。

进入大厅尽头即可完成探索。
步行速度为每秒两米，冲刺速度为每秒五米。
路线包含短距离支路，通道宽度为 4.6 米。
素材与生成来源见 ENVIRONMENT-GENERATION.json。
''',encoding='utf-8')
        quality=json.loads((candidate/'QUALITY.json').read_text());quality['entities']=len(level['nodes'])+len(level['prefabs']);write(candidate/'QUALITY.json',quality)
        for path in candidate.rglob('*'):
            if path.is_file():
                target=output/path.relative_to(candidate);target.parent.mkdir(parents=True,exist_ok=True);shutil.copy2(path,target)
    print(json.dumps(dict(output=str(output),routeMeters=meters,environmentAssets=len(environment_assets),entities=quality['entities'])))

if __name__=='__main__':
    parser=argparse.ArgumentParser(__doc__);parser.add_argument('--output',type=Path,required=True);parser.add_argument('--blender',type=Path,required=True);parser.add_argument('--character',type=Path)
    args=parser.parse_args();author(args.output,args.blender,args.character)
