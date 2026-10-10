"""Verify bounded renderer reuse, third-scene eviction and empty-scene restart."""
import argparse,json,os,secrets,shutil,subprocess,time,uuid
from pathlib import Path
from test_validation_protocol import exchange
from run_playable_long_run import NativeWindow

def run(player,output):
    out=output/('run-'+uuid.uuid4().hex[:8]);out.mkdir(parents=True)
    source=Path(__file__).resolve().parents[1]/'assets_public';results=[]
    for workflow in ('platformer','scene_inspector'):
        case=out/workflow;case.mkdir();project=case/'game'
        shutil.copytree(source/workflow,project,ignore=shutil.ignore_patterns('.azure'))
        descriptor=json.loads((project/'project.azureproject').read_text(encoding='utf-8'))
        original=descriptor['startupScene'];level=json.loads((project/'assets'/original.split(':/',1)[1]).read_text(encoding='utf-8'))
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
                '--validation-token-env','AZURE_RESIDENCY_TOKEN','--validation-endpoint',str(endpoint)],env=env,stdout=stdout,stderr=stderr,startupinfo=info)
            try:
                deadline=time.monotonic()+45
                while not endpoint.exists():
                    assert process.poll() is None and time.monotonic()<deadline;time.sleep(.02)
                address=json.loads(endpoint.read_text(encoding='utf-8'))
                def call(op,**fields):
                    result=exchange(address,token,dict(op=op,**fields));assert result['passed'],result
                    return result.get('value')
                def state():return json.loads(call('query',name='game.state'))
                def wait(reference,revision):
                    deadline=time.monotonic()+45
                    while True:
                        current=state();assert not current['loadError'] and not current['scriptErrors'] and not current['presentationErrors'],current
                        if not current['loading'] and current['levelReference']==reference and current['levelRevision']>revision:return current
                        assert process.poll() is None and time.monotonic()<deadline;time.sleep(.02)
                wait(original,0)
                for reference in ('assets:/residency-alias.azurelevel','assets:/empty.azurelevel','assets:/empty.azurelevel',original,'assets:/residency-alias.azurelevel',original):
                    previous=state()['levelRevision'];call('input',event=dict(action='load-level',reference=reference))
                    current=wait(reference,previous)
                    residency=json.loads(call('query',name='render.residency'));rows.append(residency)
                    expected='sample' if reference.endswith('empty.azurelevel') else 'character'
                    assert residency['renderer']==expected and residency['entries']<=2,residency
                window=NativeWindow(process.pid);assert window.find();window.close();assert process.wait(timeout=45)==0
            finally:
                if process.poll() is None:process.kill();process.wait()
                (case/'observations.json').write_text(json.dumps(rows,indent=2)+'\n',encoding='utf-8')
        logs=(case/'stdout.log').read_text(encoding='utf-8')+(case/'stderr.log').read_text(encoding='utf-8')
        assert 'Allocator after unload: buffers=0 images=0' in logs
        assert not any(x in logs for x in ('VUID-','Validation Error','SYNC-HAZARD'))
        results.append(dict(workflow=workflow,thirdSceneEviction=True,emptySceneRestart=True,released=True))
    (out/'summary.json').write_text(json.dumps(dict(passed=True,physicalInput=False,cases=results),indent=2)+'\n',encoding='utf-8')

if __name__=='__main__':
    p=argparse.ArgumentParser();p.add_argument('--player',type=Path,required=True);p.add_argument('--output',type=Path,required=True)
    a=p.parse_args();run(a.player.resolve(),a.output.resolve())
