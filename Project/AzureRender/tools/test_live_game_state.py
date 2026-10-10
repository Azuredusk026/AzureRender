"""Query live production state without retaining a per-frame runtime report."""
import argparse,json,os,secrets,subprocess,sys,time,uuid
from pathlib import Path
sys.path.insert(0,str(Path('tools').resolve()))
from test_validation_protocol import exchange
from run_playable_long_run import NativeWindow

def run(player,project,out):
    out=out.resolve();out.mkdir(parents=True,exist_ok=False)
    token=secrets.token_hex(32);endpoint=out/'endpoint.json';timing=out/'timing.json'
    env=dict(os.environ,AZURE_LIVE_TEST_TOKEN=token)
    info=subprocess.STARTUPINFO();info.dwFlags|=subprocess.STARTF_USESHOWWINDOW;info.wShowWindow=0
    command=[str(player.resolve()),'--project',str(project.resolve()),'--width','960','--height','540',
        '--validation-token-env','AZURE_LIVE_TEST_TOKEN','--validation-endpoint',str(endpoint),
        '--gpu-timing','--gpu-timing-output',str(timing)]
    rows=[]
    with (out/'stdout.log').open('w') as stdout,(out/'stderr.log').open('w') as stderr:
        process=subprocess.Popen(command,env=env,stdout=stdout,stderr=stderr,startupinfo=info)
        window=NativeWindow(process.pid)
        try:
            deadline=time.monotonic()+45
            while not endpoint.exists():
                assert process.poll() is None and time.monotonic()<deadline
                time.sleep(.05)
            address=json.loads(endpoint.read_text())
            def call(op,**args):
                result=exchange(address,token,dict(op=op,**args));rows.append(result)
                assert result['passed'],result
                return result.get('value')
            state=lambda:json.loads(call('query',name='game.state'))
            first=state()
            while first['loading']:
                assert time.monotonic()<deadline
                time.sleep(.05);first=state()
            assert first['characters']
            assert first['levelReference']=='assets:/platformer.azurelevel'
            p=first['characters'][0]['position']
            call('input',event=dict(action='key',key=68,down=True));time.sleep(.4)
            call('input',event=dict(action='key',key=68,down=False))
            moved=state();assert moved['characters'][0]['position'][0]>p[0]+.3
            assert moved['frame']>first['frame']
            call('input',event=dict(action='key',key=82,down=True));time.sleep(.05)
            call('input',event=dict(action='key',key=82,down=False))
            deadline=time.monotonic()+30
            while True:
                current=state()
                if current['levelRevision']>first['levelRevision'] and not current['loading']:break
                assert time.monotonic()<deadline
                time.sleep(.05)
            assert abs(current['characters'][0]['position'][0])<.3
            assert window.find();window.close();assert process.wait(timeout=45)==0
        finally:
            if process.poll() is None:process.kill();process.wait()
            (out/'requests.json').write_text(json.dumps(rows,indent=2)+'\n')
    data=json.loads(timing.read_text());assert data['runtimeTraceFrames']==0
    assert 'Allocator after unload: buffers=0 images=0' in (out/'stdout.log').read_text()
    assert 'VUID-' not in (out/'stderr.log').read_text()
    (out/'summary.json').write_text(json.dumps(dict(passed=True,realTime=True,readOnlyObservation=True,
        inputReplayed=True,restart=True,retainedRuntimeTraceFrames=0),indent=2)+'\n')

if __name__=='__main__':
    p=argparse.ArgumentParser();p.add_argument('--player',type=Path,required=True)
    p.add_argument('--project',type=Path,required=True);p.add_argument('--output',type=Path,required=True)
    a=p.parse_args();run(a.player,a.project,a.output/('run-'+uuid.uuid4().hex[:8]))
