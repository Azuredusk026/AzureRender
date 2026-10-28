"""Verify real Player reflection history ownership, submission and geometry."""
import argparse
import json
from pathlib import Path
import re
import subprocess
import numpy as np
from PIL import Image
import test_screen_space_reflections_gpu as source


def run(player,output,case,validation):
    output.mkdir(parents=True,exist_ok=False)
    aa=3 if case=='shared' else 4 if case=='multisample' else 0
    enabled=case!='off'
    fixture=dict(name=case,enabled=enabled,motion=case=='motion')
    project,_=source.prepare(output,fixture)
    level=output/'assets/scene.azurelevel';document=json.loads(level.read_text())
    document['renderSettings'].update(schemaVersion=19,antiAliasing=aa)
    source.write_json(level,document)
    if case=='motion':
        target=output/'assets/target.gltf';asset=json.loads(target.read_text());asset['animations'][0]['samplers'][0]['interpolation']='LINEAR';source.write_json(target,asset)
    args=[str(player),'--project',str(project),'--width',str(source.WIDTH),'--height',str(source.HEIGHT),
          '--capture-dir',str(output/'frames'),'--capture-frames','12','--capture-fps','12','--fixed-frame-step',
          '--qa-camera','scene','--qa-light','scene','--qa-isolation','reflection','--runtime-report',str(output/'runtime.json')]
    if case=='motion':args+=['--qa-animation']
    if case=='fixed':args+=['--disable-bindless','--disable-compute-skinning','--disable-parallel-recording']
    source.write_json(output/'command.json',args)
    shaders={p.name:source.sha(p) for p in (player.parent/'shaders').glob('*.spv')}
    inputs={p.relative_to(output).as_posix():source.sha(p) for p in (output/'assets').rglob('*') if p.is_file()}
    report=dict(schemaVersion=1,status='running',case=case,playerSha256=source.sha(player),scriptSha256=source.sha(Path(__file__)),shaders=shaders,inputSha256=inputs)
    try:
        startup=None
        if hasattr(subprocess,'STARTUPINFO'):
            startup=subprocess.STARTUPINFO();startup.dwFlags|=subprocess.STARTF_USESHOWWINDOW;startup.wShowWindow=0
        with (output/'player.log').open('w',encoding='utf-8') as log:
            subprocess.run(args,stdout=log,stderr=subprocess.STDOUT,check=True,timeout=180,startupinfo=startup)
        log=(output/'player.log').read_text(encoding='utf-8',errors='replace')
        assert not re.search(r'VUID-|Validation (?:Error|Warning)|\[validation warning\]|\[error\]',log,re.I)
        assert 'Allocator after unload: buffers=0 images=0' in log
        if validation!='any':assert ('Validation layer: enabled' in log)==(validation=='enabled')
        assert f"Character texture access: {'fixed' if case=='fixed' else 'indexed'}" in log
        manifest=json.loads((output/'frames/capture_manifest.json').read_text(encoding='utf-8-sig'));report['manifest']=manifest
        assert manifest['antiAliasing']==aa
        assert manifest['ordinaryOpaqueSnapshotPipelineCount']==manifest['ordinaryOpaquePipelineCount']
        expected=enabled and aa!=4
        assert manifest.get('screenReflectionTemporalPrepared')==expected,'Reflection temporal preparation must match actual view demand'
        assert manifest['screenReflectionTemporalActive']==expected
        assert manifest['screenReflectionHistoryValid']==expected
        if expected:assert manifest['screenReflectionHistorySubmission']>=12
        else:assert manifest['screenReflectionHistorySubmission']==0
        if expected:
            assert manifest['motionOutputActive'] and manifest['motionPreviousValidObjects']>=2
            assert manifest['screenReflectionOwnsMotion']==(aa!=3)
            assert manifest['screenReflectionMotionBytes']==source.WIDTH*source.HEIGHT*20
            assert manifest['screenReflectionReprojectionBytes']==source.WIDTH//2*(source.HEIGHT//2)*20*2
            assert manifest['temporalMotionShared']==(aa==3)
            assert manifest['temporalMaterialSamples']==(aa==3)
            assert manifest['temporalBytes']==(source.WIDTH*source.HEIGHT*52 if aa==3 else 0)
        elif case=='multisample':
            assert manifest['rasterizationSamples']==4 and not manifest['multisampleFallback']
            assert manifest['screenReflectionTemporalPreparation']=='multisample-motion-unavailable'
            assert manifest['screenReflectionMotionBytes']==0
        else:assert not manifest['motionOutputActive'] and manifest['screenReflectionMotionBytes']==0
        runtime=json.loads((output/'runtime.json').read_text());assert not runtime['scriptErrors'] and not runtime['presentationErrors']
        images=sorted((output/'frames').glob('frame_*.png'));assert len(images)==12
        last=np.asarray(Image.open(images[-1]).convert('RGB'))
        if expected:
            oracle=source.oracle(source.RIGHT if case=='motion' else source.LEFT)
            black=np.zeros_like(last);report['geometry']=source.measure_reflection(last,black,oracle,case)
            if case=='motion':
                report['trajectory']=[]
                for index,path in enumerate(images):
                    position=source.LEFT+(source.RIGHT-source.LEFT)*min(1,index/12/.25)
                    report['trajectory'].append(source.measure_reflection(np.asarray(Image.open(path).convert('RGB')),black,source.oracle(position),f'motion-frame-{index}'))
        report.update(status='passed',images={p.name:source.sha(p) for p in images})
        assert report['playerSha256']==source.sha(player)
        assert shaders=={p.name:source.sha(p) for p in (player.parent/'shaders').glob('*.spv')}
        assert all(source.sha(output/key)==value for key,value in inputs.items())
    except Exception as error:
        report.update(status='failed',error=str(error));raise
    finally:
        source.write_json(output/'result.json',report)
    print(json.dumps(dict(case=case,status=report['status'])))


if __name__=='__main__':
    parser=argparse.ArgumentParser(description=__doc__);parser.add_argument('--player',type=Path,required=True);parser.add_argument('--output',type=Path,required=True)
    parser.add_argument('--case',choices=['owned','shared','fixed','off','multisample','motion'],default='owned')
    parser.add_argument('--expected-validation',choices=['any','enabled','disabled'],default='any');args=parser.parse_args()
    run(args.player.resolve(),args.output.resolve(),args.case,args.expected_validation)
