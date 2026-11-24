"""Verify preloaded commits, cancellation and bounded renderer residency."""
import argparse
import json
from pathlib import Path
import subprocess
import shutil
from run_playable_performance import fixture, stats


def prepare(root, cycles=10):
    fixture(root)
    original=root/'assets/exploration.azurelevel'
    level=json.loads(original.read_text(encoding='utf-8'));level['id']='alternate'
    for resource in level['resources']:
        if resource['id']=='cube':resource['id']='alternate-cube'
    for node in level['nodes']:
        if node.get('resourceId')=='cube':node['resourceId']='alternate-cube'
    (root/'assets/alternate.azurelevel').write_text(json.dumps(level,indent=2),encoding='utf-8')
    novel=json.loads(json.dumps(level));novel['id']='novel-candidate'
    shutil.copy2(root/'assets/guide.gltf',root/'assets/novel-guide.gltf')
    for resource in novel['resources']:
        if resource['id']=='guide':resource['asset']='assets:/novel-guide.gltf'
    (root/'assets/novel.azurelevel').write_text(json.dumps(novel),encoding='utf-8')
    events=[{'frame':20,'action':'preload-level','reference':'assets:/missing.azurelevel'},
            {'frame':21,'action':'cancel-level'},
            {'frame':30,'action':'preload-level','reference':'assets:/novel.azurelevel'},
            {'frame':45,'action':'cancel-level'},
            {'frame':50,'action':'preload-level','reference':'assets:/alternate.azurelevel'},
            {'frame':180,'action':'preload-level','reference':'assets:/exploration.azurelevel'}]
    for i in range(cycles):
        start=400+i*600
        events += [{'frame':start,'action':'load-level','reference':'assets:/alternate.azurelevel'},
                   {'frame':start+150,'action':'key','key':82,'down':True},{'frame':start+151,'action':'key','key':82,'down':False},
                   {'frame':start+300,'action':'load-level','reference':'assets:/exploration.azurelevel'},
                   {'frame':start+450,'action':'key','key':82,'down':True},{'frame':start+451,'action':'key','key':82,'down':False}]
    task=root.parent/'cycles.input.json';task.write_text(json.dumps({'schemaVersion':1,'actions':events}),encoding='utf-8')
    return task


def verify(path, cycles=10):
    report=json.loads(path.read_text(encoding='utf-8'));resources=report['resourceFrames']
    commits=[r for r in resources if r['committed'] and r['frame']>300]
    assert len(commits)==cycles*4,(len(commits),cycles*4)
    times=stats([r['commitMs'] for r in commits])
    frame_times=stats([report['cpuFrame']['totalSamplesMs'][r['frame']-1] for r in commits])
    assert times['p99']<=8,times
    assert frame_times['p99']<=33.3,frame_times
    stable=[r for r in resources if r['frame']>=1000 and not r['committed']]
    for field in ('buffers','images','bufferBytes','imageBytes'):
        values=[r[field] for r in stable]
        assert max(values)<=min(values)*1.05,(field,min(values),max(values))
    return dict(commits=len(commits),switches=cycles*2,restarts=cycles*2,commitMilliseconds=times,
                commitFrameMilliseconds=frame_times,first=stable[0],last=stable[-1],passed=True)


def main():
    parser=argparse.ArgumentParser(__doc__);parser.add_argument('--player',type=Path,required=True)
    parser.add_argument('--output',type=Path,required=True);args=parser.parse_args()
    args.output.mkdir(parents=True,exist_ok=True);task=prepare(args.output/'game')
    timing=args.output/'cycles.json';runtime=args.output/'runtime.json'
    info=subprocess.STARTUPINFO();info.dwFlags|=subprocess.STARTF_USESHOWWINDOW;info.wShowWindow=0
    result=subprocess.run([str(args.player.resolve()),'--project',str((args.output/'game/project.azureproject').resolve()),
        '--width','1920','--height','1080','--game-actions',str(task.resolve()),'--fixed-frame-step','--smoke-frames','6600',
        '--gpu-timing','--gpu-timing-output',str(timing.resolve()),'--runtime-report',str(runtime.resolve())],
        capture_output=True,encoding='utf-8',errors='replace',timeout=240,startupinfo=info)
    (args.output/'stdout.log').write_text(result.stdout,encoding='utf-8');(args.output/'stderr.log').write_text(result.stderr,encoding='utf-8')
    assert result.returncode==0,result.stderr
    assert 'VUID-' not in result.stderr and 'Validation Error' not in result.stderr,result.stderr
    assert 'Allocator after unload: buffers=0 images=0' in result.stdout
    data=json.loads(runtime.read_text(encoding='utf-8'));assert not data['scriptErrors'] and not data['presentationErrors']
    summary=verify(timing);(args.output/'summary.json').write_text(json.dumps(summary,indent=2),encoding='utf-8');print(json.dumps(summary))


if __name__=='__main__':main()
