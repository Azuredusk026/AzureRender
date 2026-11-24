"""Measure stationary frames and motion-compensated interior continuity."""
import argparse,json,hashlib,sys
from concurrent.futures import ThreadPoolExecutor
from pathlib import Path
import numpy as np
from PIL import Image
sys.path.insert(0,str(Path(__file__).resolve().parents[1]/'build/qa-python'))
import cv2
cv2.setNumThreads(1)
from test_character_finish import capture,digest

def measure(folder,static=False,region=None):
    paths=sorted(folder.glob('frame_*.png'));assert len(paths)==120
    first=np.asarray(Image.open(paths[0]).convert('RGB'))
    previous=first;maximum=0.;worst=None;eligible=[];p95=[];means=[]
    h,w=first.shape[:2];yy,xx=np.mgrid[:h,:w].astype(np.float32)
    for index,path in enumerate(paths[1:],1):
        current=np.asarray(Image.open(path).convert('RGB'))
        if static:
            assert np.array_equal(current,first),f'Stationary frame changed: {path}'
            previous=current;continue
        a=cv2.cvtColor(previous,cv2.COLOR_RGB2GRAY);b=cv2.cvtColor(current,cv2.COLOR_RGB2GRAY)
        forward=cv2.calcOpticalFlowFarneback(a,b,None,.5,4,21,5,7,1.5,0)
        reverse=cv2.calcOpticalFlowFarneback(b,a,None,.5,4,21,5,7,1.5,0)
        rx=xx+forward[:,:,0];ry=yy+forward[:,:,1]
        warped=cv2.remap(current.astype(np.float32),rx,ry,cv2.INTER_LINEAR)
        back=cv2.remap(reverse,rx,ry,cv2.INTER_LINEAR)
        consistency=np.linalg.norm(forward+back,axis=2)<.5
        # The background is constant across each horizontal row. Keep an
        # eroded surface interior and reject silhouettes and new occlusions.
        mask=np.max(np.abs(previous.astype(np.int16)-previous[:,0:1].astype(np.int16)),axis=2)>20
        mask=cv2.erode(mask.astype(np.uint8),np.ones((9,9),np.uint8))>0
        mask&=consistency&(rx>4)&(rx<w-5)&(ry>4)&(ry<h-5)
        mask[int(h*.9):]=False
        if region is not None:mask&=region
        assert mask.sum()>100,'Insufficient stable surface interior'
        residual=np.abs(previous.astype(np.float32)-warped).mean(axis=2)
        value=float(residual[mask].mean());eligible.append(int(mask.sum()));p95.append(float(np.percentile(residual[mask],95)))
        if value>maximum:maximum=value;worst=(index,current.copy(),np.where(mask,residual,0))
        if region is not None:means.append(float(current[region].mean()))
        previous=current
    if worst:
        Image.fromarray(worst[1]).save(folder/'worst-frame.png')
        Image.fromarray(np.uint8(np.clip(worst[2]*12,0,255))).save(folder/'worst-residual.png')
        Image.fromarray(worst[1][int(h*.12):int(h*.75),int(w*.32):int(w*.68)]).resize((720,720)).save(folder/'worst-crop.png')
    result={'frames':len(paths),'stationaryExact':static,'maximumCompensatedMeanDifference255':maximum,
            'maximumCompensatedP95Difference255':max(p95,default=0),'minimumStableInteriorPixels':min(eligible,default=0),
            'worstFrame':worst[0] if worst else None}
    if means:result['maximumAdjacentRegionMeanDifference255']=float(np.max(np.abs(np.diff(means))))
    return result

def main():
    p=argparse.ArgumentParser(__doc__);p.add_argument('--executable',type=Path,required=True);p.add_argument('--asset',type=Path,required=True)
    p.add_argument('--output',type=Path,required=True);p.add_argument('--annotations',type=Path,required=True)
    p.add_argument('--resume',action='store_true')
    p.add_argument('--mode',action='append',choices=['static','light','light-left','idle','turntable','shadow-turn'])
    args=p.parse_args();root=args.output.resolve();root.mkdir(parents=True,exist_ok=True)
    spec=json.loads(args.annotations.read_text(encoding='utf-8'));assert spec['schemaVersion']==1
    report={'assetSha256':digest(args.asset),'executableSha256':digest(args.executable),'annotationsSha256':digest(args.annotations),
            'shaderHashes':{p.name:digest(p) for p in (args.executable.resolve().parent/'shaders').glob('*.spv')},
            'metric':'Farneback forward/backward consistency, eroded interior, encoded RGB residual','cases':[],'failures':[]}
    if args.resume and args.mode and (root/'summary.json').exists():
        previous=json.loads((root/'summary.json').read_text(encoding='utf-8'))
        for key in ['assetSha256','executableSha256','annotationsSha256','shaderHashes']:assert previous[key]==report[key],'Capture input changed'
        report['cases']=[case for case in previous['cases'] if case['mode'] not in args.mode]
        report['failures']=[failure for failure in previous['failures'] if not any(failure.startswith(mode+'-') for mode in args.mode)]
    jobs=[];pool=ThreadPoolExecutor(max_workers=3)
    for mode in args.mode or ['static','light','light-left','idle','turntable','shadow-turn']:
        for fps in ([60] if mode=='static' else [24,60]):
            flags=['--capture-fps',str(fps),'--anti-aliasing','supersample','--set','render.shadowDistance=10','--gpu-timing']
            if mode.startswith('light'):flags+=['--qa-light-scan']
            if mode=='idle':flags+=['--qa-animation']
            if mode in ['turntable','shadow-turn']:flags+=['--portfolio']
            camera='full-body-front' if mode in ['turntable','shadow-turn'] else ('face-three-quarter-left' if mode=='light-left' else 'face-front')
            folder=root/f'{mode}-{fps}'
            if not (args.resume and folder.exists()):
                folder=capture(args.executable.resolve(),args.asset.resolve(),root,f'{mode}-{fps}',camera,'stylized-key',isolation='shadow-visibility' if mode=='shadow-turn' else 'beauty',frames=120,extra=flags)
            region=None
            if mode.startswith('light'):region=np.asarray(Image.open(args.annotations.parent/spec['faceLeftRegion' if mode=='light-left' else 'faceRegion']).convert('L'))>200
            jobs.append((mode,fps,folder,pool.submit(measure,folder,mode=='static',region)))
    print('All GPU captures complete; measuring frozen sequences',flush=True)
    for mode,fps,folder,future in jobs:
            result=future.result();result.update(mode=mode,fps=fps)
            manifest=json.loads((folder/'capture_manifest.json').read_text(encoding='utf-8'))
            assert manifest['antiAliasing']==2 and manifest['fps']==fps and manifest['sceneRenderWidth']==2560
            if result['maximumCompensatedMeanDifference255']>spec['maximumCompensatedMeanDifference255']:report['failures'].append(f'{mode}-{fps}: interior continuity')
            if result.get('maximumAdjacentRegionMeanDifference255',0)>spec['maximumAdjacentRegionMeanDifference255']:report['failures'].append(f'{mode}-{fps}: face light continuity')
            report['cases'].append(result);(root/'summary.json').write_text(json.dumps(report,indent=2),encoding='utf-8');print(json.dumps(result),flush=True)
    pool.shutdown()
    assert {p.name:digest(p) for p in (args.executable.resolve().parent/'shaders').glob('*.spv')}==report['shaderHashes'],'Shader input changed'
    assert digest(args.asset)==report['assetSha256'] and digest(args.executable)==report['executableSha256'],'Capture input changed'
    report['passed']=not report['failures'];(root/'summary.json').write_text(json.dumps(report,indent=2),encoding='utf-8')
    assert report['passed'],report['failures']
if __name__=='__main__':main()
