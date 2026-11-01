"""Verify scene restart, rejected reuse, bounded residency and eviction."""
import argparse,copy,json,math,os,secrets,shutil,struct,subprocess,time,uuid
from pathlib import Path
from test_validation_protocol import exchange
from run_playable_long_run import NativeWindow

def run(player,output):
    out=output/('run-'+uuid.uuid4().hex[:8]);out.mkdir(parents=True)
    source=Path(__file__).resolve().parents[1]/'assets_public';results=[]
    for workflow,template,fogged in (('fogged_scene_inspector','scene_inspector',True),
                                    ('captured_fog_scene_inspector','scene_inspector',True),
                                    ('platformer','platformer',False),('scene_inspector','scene_inspector',False)):
        case=out/workflow;case.mkdir();project=case/'game'
        shutil.copytree(source/template,project,ignore=shutil.ignore_patterns('.azure'))
        descriptor=json.loads((project/'project.azureproject').read_text(encoding='utf-8'))
        original=descriptor['startupScene'];level=json.loads((project/'assets'/original.split(':/',1)[1]).read_text(encoding='utf-8'))
        if fogged:
            settings=level.setdefault('renderSettings',{});settings.update(antiAliasing=3,schemaVersion=max(18,settings.get('schemaVersion',1)))
            level['viewFog']=dict(schemaVersion=2,asset='',enabled=True,captureView=None,
                projection=dict(schemaVersion=1,extent=[960,540],pixelOrigin='upper-left',framePhase=0,
                    pixelCenterOffset=.5,jitterRadius=0,depthEncoding=[0,1,1],minimumViewDepth=0,nearFadeWidth=.000001),
                analytic=dict(schemaVersion=1,aerial=None,height=dict(schemaVersion=1,
                    layers=[dict(density=.001,heightFalloff=0,referenceHeight=0,scatteringColor=[.1,.1,.1])],
                    startViewDepth=0,minimumTransmittance=0,transmittanceBias=[0,0,0,0])))
            if workflow=='captured_fog_scene_inspector':
                cameras=[n for n in level['nodes'] if 'azure.camera' in n.get('components',{})]
                if not cameras:
                    camera=dict(id='residency-capture-camera',name='residency-capture-camera',parentId='',resourceId='',components={
                        'azure.transform':dict(type='azure.transform',version=1,data=dict(translation=[0,0,3],rotation=[0,0,0],scale=[1,1,1])),
                        'azure.camera':dict(type='azure.camera',version=2,data=dict(enabled=True,priority=1,fieldOfView=55,nearPlane=.1,farPlane=100))})
                    level['nodes'].append(camera);cameras.append(camera)
                assert len(cameras)==1
                camera=cameras[0];camera['parentId']=''
                camera['components']['azure.transform']['data'].update(translation=[0,0,3],rotation=[0,0,0])
                camera['components']['azure.camera']['data'].update(enabled=True,fieldOfView=55,nearPlane=.1,farPlane=100)
                binding=level['viewFog'];binding['schemaVersion']=1;binding.pop('analytic');binding['asset']='assets:/view.azfog'
                binding['captureView']=dict(projection='perspective',position=[0,0,3],target=[0,0,2],up=[0,1,0],fieldOfViewRadians=math.radians(55),orthographicHeight=5,nearPlane=.1,farPlane=100,projectionOffset=[0,0])
                (project/'assets/view.azfog').write_bytes(b'AZFOG001'+struct.pack('<3I4e',1,1,1,.03,.05,.07,.5))
                changed=copy.deepcopy(level);changed['viewFog']['captureView']['position'][0]=.25
                (project/'assets/changed-fog.azurelevel').write_text(json.dumps(changed),encoding='utf-8')
                disabled=copy.deepcopy(level);disabled['viewFog']['enabled']=False
                (project/'assets/disabled-fog.azurelevel').write_text(json.dumps(disabled),encoding='utf-8')
            (project/'assets'/original.split(':/',1)[1]).write_text(json.dumps(level),encoding='utf-8')
        mesh=project/'assets'/level['resources'][0]['asset'].split(':/',1)[1]
        alias=mesh.with_name('residency-alias'+mesh.suffix);shutil.copyfile(mesh,alias)
        level['resources'][0]['asset']='assets:/'+alias.relative_to(project/'assets').as_posix()
        (project/'assets/residency-alias.azurelevel').write_text(json.dumps(level),encoding='utf-8')
        (project/'assets/empty.azurelevel').write_text(json.dumps(dict(schemaVersion=1,id='empty',sceneType='sample',resources=[],nodes=[],lights=[])),encoding='utf-8')
        endpoint=case/'endpoint.json';token=secrets.token_hex(32);rows=[]
        env=dict(os.environ,AZURE_RESIDENCY_TOKEN=token,VK_INSTANCE_LAYERS='VK_LAYER_KHRONOS_validation',VK_LAYER_VALIDATE_SYNC='1')
        info=subprocess.STARTUPINFO();info.dwFlags|=subprocess.STARTF_USESHOWWINDOW;info.wShowWindow=0
        with (case/'stdout.log').open('w',encoding='utf-8') as stdout,(case/'stderr.log').open('w',encoding='utf-8') as stderr:
            process=subprocess.Popen([str(player),'--project',str(project/'project.azureproject'),'--width','960','--height','540',
                '--validation-token-env','AZURE_RESIDENCY_TOKEN','--validation-endpoint',str(endpoint)],cwd=case,env=env,stdout=stdout,stderr=stderr,startupinfo=info)
            try:
                deadline=time.monotonic()+45
                while not endpoint.exists():
                    assert process.poll() is None and time.monotonic()<deadline;time.sleep(.02)
                address=json.loads(endpoint.read_text(encoding='utf-8'))
                def call(op,*,io_timeout=None,**fields):
                    result=exchange(address,token,dict(op=op,**fields),io_timeout=io_timeout);assert result['passed'],result
                    return result.get('value')
                def state(io_timeout=None):return json.loads(call('query',name='game.state',io_timeout=io_timeout))
                def wait(reference,revision):
                    deadline=time.monotonic()+45
                    while True:
                        remaining=deadline-time.monotonic();assert remaining>0 and process.poll() is None
                        current=state(io_timeout=remaining);assert time.monotonic()<=deadline
                        assert not current['loadError'] and not current['scriptErrors'] and not current['presentationErrors'],current
                        if not current['loading'] and current['levelReference']==reference and current['levelRevision']>revision:return current
                        assert process.poll() is None and time.monotonic()<deadline;time.sleep(.02)
                wait(original,0)
                initial_residency=json.loads(call('query',name='render.residency'))
                references=[original,'assets:/residency-alias.azurelevel','assets:/empty.azurelevel','assets:/empty.azurelevel',original,'assets:/residency-alias.azurelevel',original]
                if workflow=='captured_fog_scene_inspector':references[1:1]=['assets:/changed-fog.azurelevel',original,'assets:/disabled-fog.azurelevel',original]
                for index,reference in enumerate(references):
                    previous=state()['levelRevision'];call('input',event=dict(action='load-level',reference=reference))
                    current=wait(reference,previous)
                    residency=json.loads(call('query',name='render.residency'));rows.append(residency)
                    expected='sample' if reference.endswith('empty.azurelevel') else 'character'
                    assert residency['renderer']==expected and residency['entries']<=2,residency
                    if index==0:
                        assert residency['activeResourceKey']==initial_residency['activeResourceKey']
                        assert residency['entries']==initial_residency['entries'],('Unchanged fog binding must reuse the active renderer',initial_residency,residency)
                    if reference in ('assets:/changed-fog.azurelevel','assets:/disabled-fog.azurelevel'):
                        assert residency['activeResourceKey']!=initial_residency['activeResourceKey'],('Changed fog binding requires a distinct resource identity',reference,residency)
                window=NativeWindow(process.pid);assert window.find();window.close();assert process.wait(timeout=45)==0
            finally:
                if process.poll() is None:
                    try:
                        window=NativeWindow(process.pid)
                        if window.find():window.close();process.wait(timeout=45)
                        else:process.kill();process.wait()
                    except Exception:process.kill();process.wait()
                (case/'observations.json').write_text(json.dumps(rows,indent=2)+'\n',encoding='utf-8')
        logs=(case/'stdout.log').read_text(encoding='utf-8')+(case/'stderr.log').read_text(encoding='utf-8')
        assert 'Allocator after unload: buffers=0 images=0' in logs
        assert not any(x in logs for x in ('VUID-','Validation Error','SYNC-HAZARD'))
        diagnostics=[json.loads(line) for line in (case/'captures/azureplayer.log.jsonl').read_text(encoding='utf-8').splitlines()]
        assert any(r['message']=='Allocator after unload: buffers=0 images=0' for r in diagnostics)
        assert not any(r['level']=='error' or r['subsystem']=='validation' and r['level']=='warning' for r in diagnostics)
        results.append(dict(workflow=workflow,sameSceneRestart=True,unchangedBindingReused=True,changedBindingPrepared=workflow=='captured_fog_scene_inspector',thirdSceneEviction=True,emptySceneRestart=True,released=True))
    (out/'summary.json').write_text(json.dumps(dict(passed=True,physicalInput=False,cases=results),indent=2)+'\n',encoding='utf-8')

if __name__=='__main__':
    p=argparse.ArgumentParser();p.add_argument('--player',type=Path,required=True);p.add_argument('--output',type=Path,required=True)
    a=p.parse_args();run(a.player.resolve(),a.output.resolve())
