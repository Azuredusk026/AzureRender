import hashlib,json
from pathlib import Path
import numpy as np
from PIL import Image
run=Path(__file__).resolve().parent
regions={'hair-crown':[710,400,820,550],'hair-fringe':[700,570,800,650],'face-cheek':[705,852,795,885],'face-chin':[770,960,810,985],'eye-left':[625,784,683,814],'environment-right-wall':[1250,300,1450,450],'environment-left-wall':[30,400,230,650]}
linear=lambda value:np.where(value/255<=.04045,value/255/12.92,((value/255+.055)/1.055)**2.4)
luma=np.array([.21267290413379669,.7151522040367126,.07217500358819962]);pairs=[]
for component in ['ambient','direct-diffuse','hair-kk','object-shadow-visibility','face-sdf']:
    paths={label:run/'captures'/(label+'-'+component)/'frames/frame_000000.png' for label in ['joint','reference-look']}
    if not all(p.exists() and (p.parent/'capture_manifest.json').exists() for p in paths.values()):continue
    a,b=[np.array(Image.open(paths[k]).convert('RGB')) for k in paths];rows=[]
    for name,(x0,y0,x1,y1) in regions.items():
        av=a[y0:y1,x0:x1];bv=b[y0:y1,x0:x1];delta=np.abs(av.astype(int)-bv.astype(int));am=float(np.median(linear(av)@luma));bm=float(np.median(linear(bv)@luma))
        rows.append({'region':name,'bounds':[x0,y0,x1,y1],'jointMedianLinearDisplay':am,'referenceMedianLinearDisplay':bm,'jointReferenceRatio':am/bm if bm>1e-8 else None,'jointClippedPixels':int(np.any(av==255,axis=2).sum()),'referenceClippedPixels':int(np.any(bv==255,axis=2).sum()),'exactPixelsFraction':float(np.all(av==bv,axis=2).mean()),'maximumChannelError':int(delta.max()),'meanAbsoluteChannelError':float(delta.mean())})
    pairs.append({'component':component,'images':{k:{'path':str(p),'sha256':hashlib.sha256(p.read_bytes()).hexdigest()} for k,p in paths.items()},'regions':rows})
report={'status':'partial' if len(pairs)<5 else 'measured','scope':'Same geometry, camera and pose. Radiometric images share -2EV and empty grade LUT. Linear-display values are LDR diagnostic measurements; clipped/near-zero regions are explicitly recorded. KK mode multiplies the two lobes by three before exposure. Visibility and SDF modes are unitless. Rectangles are diagnostic regions, not final material masks.','pairs':pairs}
(run/'component-comparison.json').write_text(json.dumps(report,indent=2)+'\n')
print(json.dumps({'pairs':len(pairs),'actorRatios':{p['component']:{x['region']:x['jointReferenceRatio'] for x in p['regions'] if not x['region'].startswith('environment')} for p in pairs},'environmentExactFractions':{p['component']:{x['region']:x['exactPixelsFraction'] for x in p['regions'] if x['region'].startswith('environment')} for p in pairs}},indent=2))
