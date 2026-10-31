"""Validate the projected current-frame producer through actual Player scenes."""
from pathlib import Path
import argparse
import json
import subprocess
import sys
import numpy as np
import test_screen_space_reflections_gpu as lab


def main():
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--executable',type=Path,required=True)
    parser.add_argument('--output',type=Path)
    parser.add_argument('--smoke',action='store_true')
    parser.add_argument('--expected-validation',choices=('any','enabled','disabled'),default='any')
    args=parser.parse_args();engine=Path(__file__).resolve().parents[1]
    run=args.output.resolve() if args.output else Path(subprocess.check_output([sys.executable,str(engine/'tools/artifact_cleanup.py'),'--new-run','projected-scene-gpu'],cwd=engine,text=True).strip())
    run.mkdir(parents=True,exist_ok=True);report=dict(status='running',captures=[],metrics=[],executableSha256=lab.sha(args.executable),budgets=lab.BUDGETS)
    try:
        cases=[dict(name='off',enabled=False),dict(name='on',algorithm='projected')]
        if not args.smoke:cases.extend([dict(name='hidden',algorithm='projected',hidden=True),dict(name='rough',algorithm='projected',roughness=.9),dict(name='rough-off',enabled=False,roughness=.9),
            dict(name='beauty',algorithm='projected',isolation='beauty'),dict(name='motion',algorithm='projected',motion=True,frames=3),
            dict(name='aa3-off',enabled=False,antiAliasing=3,frames=3),dict(name='aa3',algorithm='projected',antiAliasing=3,frames=3),
            dict(name='aa4-off',enabled=False,antiAliasing=4,frames=2),dict(name='aa4',algorithm='projected',antiAliasing=4,frames=2)])
        all_frames={}
        for backend in (('indexed',) if args.smoke else ('indexed','fixed')):
            images={}
            for case in cases:
                path=run/(backend+'-'+case['name']);frames,record=lab.capture(args.executable,path,backend,case,False,args.expected_validation)
                report['captures'].append(record);images[case['name']]=frames
                manifest=json.loads((path/'frames/capture_manifest.json').read_text(encoding='utf-8'))
                diagnostic={key:value for key,value in manifest.items() if key.startswith('screenReflection')}
                if case.get('antiAliasing')==4:assert manifest['rasterizationSamples']==4 and not manifest['multisampleFallback']
                if case.get('antiAliasing')==3:assert manifest['antiAliasingPipeline']=='native-hdr-temporal-v4' and manifest['temporalHistorySubmission']>0
                if case.get('algorithm')=='projected':
                    assert manifest['screenReflectionComputePrepared'] and manifest['screenReflectionComputeActive'],diagnostic
                    assert manifest['screenReflectionAlgorithm']=='projected' and manifest['screenReflectionProducer']=='projected-current-frame',diagnostic
                    assert not manifest['screenReflectionTemporalPrepared'] and not manifest['screenReflectionHistoryValid'],diagnostic
                    assert manifest['screenReflectionWidth']==160 and manifest['screenReflectionHeight']==120,diagnostic
                    record['producer']=dict(algorithm=manifest['screenReflectionAlgorithm'],mode=manifest['screenReflectionProducer'],historyValid=False)
            expected=lab.oracle(lab.LEFT);off=images['off'][0]
            positive=lab.measure_reflection(images['on'][0],off,expected,backend+'-projected');report['metrics'].append(positive)
            all_frames[backend]=images
            if args.smoke:continue
            mask=lab.polygon_mask(expected['reflectionPolygon'],.7)
            for name in ('hidden','rough'):
                baseline=images['rough-off'][0] if name=='rough' else off
                signal=float(lab.red_signal(images[name][0],baseline)[mask].mean());assert signal<positive['meanRedSignal']*lab.BUDGETS['maximumSuppressedSignalRatio'],(name,signal)
                report['metrics'].append(dict(name=backend+'-'+name,meanRedSignal=signal))
            difference=float(np.abs(lab.linear(images['beauty'][0])[mask]-lab.linear(images['on'][0])[mask]).mean())
            assert difference<lab.BUDGETS['maximumBeautyIsolationMeanLinearError'],(backend,'beauty reflection differs from isolation',difference)
            source=lab.linear(images['beauty'][0])[lab.polygon_mask(expected['sourcePolygon'],.55)].mean(axis=0)
            assert source[0]>.05 and source[0]>2*max(source[1],source[2]),(backend,'full source color is absent',source.tolist())
            report['metrics'].append(dict(name=backend+'-source-and-beauty',sourceLinear=source.tolist(),receiverMeanLinearError=difference))
            for mode in ('aa3','aa4'):
                report['metrics'].append(lab.measure_reflection(images[mode][-1],images[mode+'-off'][-1],expected,backend+'-'+mode))
            motion=[lab.measure_reflection(images['motion'][index],off,lab.oracle(x),backend+'-motion-'+str(index)) for index,x in [(0,lab.LEFT),(2,lab.RIGHT)]]
            observed=np.array(motion[1]['weightedCentroid'])-np.array(motion[0]['weightedCentroid']);wanted=np.array(lab.oracle(lab.RIGHT)['reflectionCenter'])-np.array(lab.oracle(lab.LEFT)['reflectionCenter'])
            assert observed[0]>20 and np.linalg.norm(observed-wanted)<12,(backend,observed.tolist(),wanted.tolist());report['metrics'].extend(motion)
        if not args.smoke:
            mask=np.ones((lab.HEIGHT,lab.WIDTH),bool)
            for case in cases:
                for i,(left,right) in enumerate(zip(all_frames['indexed'][case['name']],all_frames['fixed'][case['name']])):
                    report['metrics'].append(lab.compatible(left,right,mask,'descriptor-parity-'+case['name']+'-'+str(i)))
        report['status']='passed'
        if not args.output:subprocess.run([sys.executable,str(engine/'tools/artifact_cleanup.py'),'--finish',str(run)],check=True)
        print('Projected scene cases passed:',len(report['captures']),run)
    except Exception as error:report.update(status='failed',error=str(error));raise
    finally:(run/'summary.json').write_text(json.dumps(report,indent=2)+'\n',encoding='utf-8')


if __name__=='__main__':main()
