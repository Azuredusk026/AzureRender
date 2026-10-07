"""Verify input replay does not allocate an unrequested runtime trace."""
import argparse
import json
from pathlib import Path
import subprocess
import tempfile
from run_playable_performance import fixture

def run(player,output):
    output.mkdir(parents=True,exist_ok=True);project=output/'project';fixture(project,stress=True)
    actions=output/'actions.json';actions.write_text(json.dumps({'schemaVersion':1,'actions':[
        {'frame':20,'action':'key','key':68,'down':True},{'frame':50,'action':'key','key':68,'down':False}]}),encoding='utf-8')
    for requested in (False,True):
        name='requested' if requested else 'unrequested';timing=output/(name+'.json');runtime=output/(name+'-runtime.json')
        command=[str(player.resolve()),'--project',str(project/'project.azureproject'),'--game-actions',str(actions),
            '--width','960','--height','540','--fixed-frame-step','--smoke-frames','96','--gpu-timing','--gpu-timing-output',str(timing)]
        if requested:command+=['--runtime-report',str(runtime)]
        result=subprocess.run(command,cwd=output,capture_output=True,encoding='utf-8',errors='replace',timeout=120)
        (output/(name+'.stdout.log')).write_text(result.stdout,encoding='utf-8');(output/(name+'.stderr.log')).write_text(result.stderr,encoding='utf-8')
        assert result.returncode==0,result.stdout+result.stderr
        assert 'Allocator after unload: buffers=0 images=0' in result.stdout
        data=json.loads(timing.read_text(encoding='utf-8'))
        assert data['samples']==96 and data['sceneTriangles']>0 and data['skinnedVertices']>0
        retained=data['runtimeTraceFrames']
        if requested:
            report=json.loads(runtime.read_text(encoding='utf-8'))
            assert retained==len(report['routeFrames']) and retained>0
            assert report['replayedActions']==2 and not report['scriptErrors']
            assert any(row['characters'] for row in report['routeFrames'])
        else:assert retained==0,f'Unrequested runtime trace retained {retained} frames'
    print(json.dumps({'status':'passed','inputReplay':True,'unrequestedFrames':0,'requestedTracePreserved':True}))
def main():
    parser=argparse.ArgumentParser(__doc__);parser.add_argument('--player',type=Path,required=True);parser.add_argument('--output',type=Path)
    args=parser.parse_args()
    with tempfile.TemporaryDirectory(prefix='azure_optional_trace_') as temporary:
        run(args.player,args.output.resolve() if args.output else Path(temporary))
if __name__=='__main__':main()
