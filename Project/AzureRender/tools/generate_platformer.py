"""Generate a platform course from measured production physics."""
import argparse,hashlib,json,math,shutil,struct,subprocess,wave
from pathlib import Path
ROOT=Path(__file__).resolve().parents[1]
def component(name,data):
    return dict(type=name,version={'azure.rigid-body':3,'azure.character':4,'azure.animator':3}.get(name,1),data=data)
def write(path,data):path.write_text(json.dumps(data,ensure_ascii=False,indent=2)+'\n',encoding='utf-8')
def tone(path,frequencies,duration,volume):
    rate=48000;samples=[]
    for i in range(round(rate*duration)):
        t=i/rate;f=frequencies[min(len(frequencies)-1,int(t/duration*len(frequencies)))];envelope=min(1,t/.015)*max(0,1-t/duration)**1.8
        samples.append(round(32767*volume*envelope*math.sin(2*math.pi*f*t)))
    with wave.open(str(path),'wb') as out:out.setparams((1,2,rate,0,'NONE','not compressed'));out.writeframes(struct.pack('<'+'h'*len(samples),*samples))
def generate(output,measurements,blender):
    m=json.loads(measurements.read_text());assert m['schemaVersion']==1 and m['maximumDistance']>1 and m['jumpHeight']>.5
    output.mkdir(parents=True,exist_ok=True);assets=output/'assets';assets.mkdir(exist_ok=True);env=assets/'environment';env.mkdir(exist_ok=True)
    source=ROOT/'assets_public/exploration';templates=ROOT/'tools/templates/platformer'
    for name in ['hero.gltf','locomotion.json','CHARACTER-LICENSE.txt']:shutil.copy2(source/'assets'/name,assets/name)
    for name in ['rock.glb','arch.glb','stone-albedo.png','stone-normal.png','stone-orm.png']:shutil.copy2(source/'assets/environment'/name,env/name)
    for path in templates.iterdir():shutil.copy2(path,assets/path.name)
    result=subprocess.run([str(blender),'-b','--factory-startup','-t','2','--python',str(ROOT/'tools/blender_platformer.py'),'--',str(env.resolve())],capture_output=True,text=True,encoding='utf-8',errors='replace',timeout=180)
    (output/'BLENDER-GENERATION.log').write_text(result.stdout+result.stderr,encoding='utf-8');assert result.returncode==0
    ratios=[.5,.55,.62,.65,.67,.78,.60];tops=[0,0,.15,.15,.35,.35,.35,.35];platforms=[];z=0
    for i,top in enumerate(tops):
        half=3 if i in [0,7] else 2
        if i:z-=platforms[-1]['halfLength']+ratios[i-1]*m['maximumDistance']+half
        p=dict(id='platform-'+str(i),x=0,z=z,top=top,halfWidth=3 if i<3 else 2.4,halfLength=half,gapRatio=ratios[i-1] if i else 0)
        if i in [3,5]:
            start=[-.65 if i==3 else 0,top-.35,z];finish=[.65 if i==3 else 0,top-.35+(.35 if i==5 else 0),z]
            distance=math.dist(start,finish);speed=.3 if i==3 else .18;hold=1.5
            p['motion']=dict(start=start,finish=finish,speed=speed,hold=hold,distance=distance,period=2*(distance/speed+hold))
        platforms.append(p)
    level=dict(schemaVersion=1,id='platformer',sceneType='character',renderSettings=dict(platform=False,cameraNear=.1,cameraFar=200,shadowDistance=65),resources=[],nodes=[],lights=[])
    for name in ['hero','platform','practice','moving','challenge','coin','rock','arch']:level['resources'].append(dict(id=name,asset='assets:/hero.gltf' if name=='hero' else f'assets:/environment/{name}.glb'))
    def node(id,resource,position,scale=(1,1,1),extra=None):
        n=dict(id=id,resourceId=resource,components={'azure.transform':component('azure.transform',dict(translation=position,scale=list(scale)))})
        if extra:
            n['components'].update({key:component(key,value) for key,value in extra.items()})
            if 'azure.rigid-body' in extra:n['components']['azure.rigid-body']['version']=3
            if 'azure.character' in extra:n['components']['azure.character']['version']=4
            if 'azure.animator' in extra:n['components']['azure.animator']['version']=3
        level['nodes'].append(n);return n
    for i,p in enumerate(platforms):
        motion=p.get('motion');pos=motion['start'] if motion else [0,p['top']-.35,p['z']]
        node(p['id'],'moving' if motion else 'challenge' if i==6 else 'practice' if i<3 else 'platform',pos,[p['halfWidth'],.35,p['halfLength']],{'azure.rigid-body':dict(halfExtent=[1,1,1],kinematic=bool(motion)),**({'azure.script':dict(asset='assets:/motion.lua')} if motion else {})})
        if not motion:node('cliff-'+str(i),'rock',[0,p['top']-4.6,p['z']],[p['halfWidth']*.6,2,p['halfLength']*.6])
        node('coin-'+str(i),'coin',[0,p['top']+.75,p['z']-(1.2 if i==0 else 0)],[1,1,1],{'azure.rigid-body':dict(trigger=True,halfExtent=[.55,.8,.55]),'azure.collectible':dict(category='coin'),'azure.script':dict(asset='assets:/coin.lua')})
    for i in [2,6]:
        p=platforms[i];node('bonus-'+str(i),'coin',[1.6,p['top']+.75,p['z']],[1,1,1],{'azure.rigid-body':dict(trigger=True,halfExtent=[.55,.8,.55]),'azure.collectible':dict(category='coin'),'azure.script':dict(asset='assets:/coin.lua')})
    motion={p['id']:p['motion'] for p in platforms if 'motion' in p}
    def lua(value):
        if isinstance(value,dict):return '{'+','.join('['+json.dumps(k)+']='+lua(v) for k,v in value.items())+'}'
        if isinstance(value,list):return '{'+','.join(lua(v) for v in value)+'}'
        return str(value)
    (assets/'motion.lua').write_text('local paths='+lua(motion)+'\n'+(templates/'motion.lua').read_text(),encoding='utf-8')
    node('hero','hero',[0,.02,0],extra={'azure.character':dict(speed=m['walkSpeed'],sprintMultiplier=m['sprintSpeed']/m['walkSpeed'],centerOffset=.9,forwardYaw=180),'azure.animator':dict(asset='assets:/locomotion.json',locomotion=True,referenceSpeed=2),'azure.task-state':dict(started=True),'azure.checkpoint':dict(position=[0,.02,0]),'azure.script':dict(asset='assets:/hero.lua')})
    node('camera','',[0,0,0],extra={'azure.third-person-camera':dict(target='hero',distance=6,targetHeight=1.2)})
    node('hud','',[0,0,0],extra={'azure.game-ui':dict(asset='assets:/hud.rml')})
    p=platforms[4];node('checkpoint','',[0,p['top']+.8,p['z']],extra={'azure.rigid-body':dict(trigger=True,halfExtent=[1.6,1.3,1]),'azure.checkpoint':dict(position=[0,p['top']+.05,p['z']]),'azure.script':dict(asset='assets:/checkpoint.lua')})
    end=platforms[-1];node('finish-arch','arch',[0,end['top'],end['z']]);node('goal','',[0,end['top']+1,end['z']],extra={'azure.rigid-body':dict(trigger=True,halfExtent=[1.8,1.8,.3]),'azure.door':dict(requiredCount=1),'azure.script':dict(asset='assets:/goal.lua')})
    for i,p in enumerate(platforms):
        n='light-'+str(i);node(n,'',[0,p['top']+2,p['z']]);level['lights'].append(dict(id=n,nodeId=n,color=[.3,.75,1] if i==7 else [1,.7,.35],intensity=2,radius=4))
    audio=assets/'audio';audio.mkdir(exist_ok=True)
    for name,freq,duration,volume in [('jump',[260,390],.16,.35),('land',[100,75],.12,.25),('coin',[660,880],.18,.3),('respawn',[260,180,300],.35,.3),('finish',[440,550,660,880],.65,.35)]:
        tone(audio/(name+'.wav'),freq,duration,volume);node('audio-'+name,'',[0,0,0],extra={'azure.audio-source':dict(asset='assets:/audio/'+name+'.wav',volume=.65)})
    project=json.loads((source/'project.azureproject').read_text());project.update(id='9498d1a5-8f30-44e5-bd77-f6058ca2412b',name='Azure Sky Steps',startupScene='assets:/platformer.azurelevel');project['runtime']['systems']=[s for s in project['runtime']['systems'] if s['id'] not in ['exploration-policy','interaction']]
    write(assets/'platformer.azurelevel',level);write(output/'project.azureproject',project)
    write(output/'QUALITY.json',dict(schemaVersion=1,entities=len(level['nodes']),platforms=8,coins=10,animatedCharacters=1,pointLights=8,shadowCascades=4,width=1920,height=1080,fullGameUi=True))
    write(output/'PLATFORMER-DESIGN.json',dict(schemaVersion=1,measurement=m,platforms=platforms,coins=10,requiredCoins=0,checkpoint='platform-4',difficulty=['认识机制','安全练习','水平平台','组合运用','垂直平台','最终挑战','通关奖励']))
    write(output/'GENERATION.json',dict(schemaVersion=1,license='CC0-1.0',measurementSha256=hashlib.sha256(measurements.read_bytes()).hexdigest(),generators={name:hashlib.sha256((ROOT/'tools'/name).read_bytes()).hexdigest() for name in ['generate_platformer.py','blender_platformer.py']},outputs={p.relative_to(output).as_posix():hashlib.sha256(p.read_bytes()).hexdigest() for p in sorted(assets.rglob('*')) if p.is_file() and p.suffix!='.azmeta'}))
    (output/'LICENSE.txt').write_text('Original Azure platformer models, textures, scripts and synthesized sounds: CC0-1.0\n')
    print(json.dumps(dict(platforms=len(platforms),length=-z,output=str(output))))
def link_exploration(output,exploration):
    source=output/'assets';target=exploration/'assets/platformer'
    for path in source.rglob('*'):
        if not path.is_file() or path.suffix=='.azmeta':continue
        out=target/path.relative_to(source);out.parent.mkdir(parents=True,exist_ok=True)
        if path.suffix in ['.azurelevel','.lua']:out.write_text(path.read_text(encoding='utf-8').replace('assets:/','assets:/platformer/'),encoding='utf-8')
        else:shutil.copy2(path,out)
    write(exploration/'PLATFORMER-LINK.json',dict(schemaVersion=1,sourceProject='Azure Sky Steps',entry='assets:/platformer/platformer.azurelevel',outputs={p.relative_to(exploration).as_posix():hashlib.sha256(p.read_bytes()).hexdigest() for p in sorted(target.rglob('*')) if p.is_file() and p.suffix!='.azmeta'}))
if __name__=='__main__':
    p=argparse.ArgumentParser(__doc__);p.add_argument('--output',type=Path,required=True);p.add_argument('--measurements',type=Path,required=True);p.add_argument('--blender',type=Path,required=True);p.add_argument('--exploration',type=Path);a=p.parse_args();generate(a.output,a.measurements,a.blender)
    if a.exploration:link_exploration(a.output,a.exploration)
