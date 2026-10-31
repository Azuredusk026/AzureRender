import hashlib,json
from pathlib import Path
import numpy as np
from PIL import Image
run=Path(__file__).resolve().parent
paths={'reference':run/'material-capture-2/reference/frames/frame_000031.png','current-head':run/'material-capture-3/current-head/frames/frame_000031.png','visibility-one':run/'material-capture-3/current-head-visibility-one/frames/frame_000031.png'}
paths['default-rim']=run/'material-capture-3/current-head-default-rim/frames/frame_000031.png'
arrays={k:np.array(Image.open(v).convert('RGB')) for k,v in paths.items()}
regions={'head':[591,62,693,180],'face-interior':[626,135,650,152],'hair-crown':[614,90,667,113],'left-eye':[618,126,633,135],'right-eye':[643,126,659,135]}
report={'scope':'Identical static geometry, native source camera/lights and temporal AA32; only face/hair documents differ after explicit static-frame and original sparse rim-policy application. Rectangles are diagnostics, not final visibility masks.','inputs':{k:{'path':str(v),'sha256':hashlib.sha256(v.read_bytes()).hexdigest()} for k,v in paths.items()},'comparisons':[]}
for label in ['current-head','visibility-one','default-rim']:
    for name,bounds in {'whole':[0,0,1280,720],**regions}.items():
        x0,y0,x1,y1=bounds;a=arrays['reference'][y0:y1,x0:x1];b=arrays[label][y0:y1,x0:x1];d=np.abs(a.astype(int)-b.astype(int))
        linear=lambda value:np.where(value/255<=.04045,value/255/12.92,((value/255+.055)/1.055)**2.4)
        y=lambda value:linear(value)@np.array([.2126,.7152,.0722])
        am=float(np.median(y(a)));bm=float(np.median(y(b)))
        report['comparisons'].append({'case':label,'region':name,'bounds':bounds,'exactPixelFraction':float(np.all(a==b,axis=2).mean()),'meanAbsoluteChannelError':float(d.mean()),'maxChannelError':int(d.max()),'referenceMedianLuminance':am,'candidateMedianLuminance':bm,'medianLuminanceRelativeError':abs(bm-am)/max(am,1e-10)})
    report.setdefault('visibilityComparison',{})[label]=hashlib.sha256(arrays[label].tobytes()).hexdigest()
report['currentVsOneExact']=bool(np.array_equal(arrays['current-head'],arrays['visibility-one']))
rim_delta=np.abs(arrays['current-head'].astype(int)-arrays['default-rim'].astype(int));report['rimPolicyDifference']={'changedPixels':int(np.any(rim_delta,axis=2).sum()),'maxChannelError':int(rim_delta.max()),'meanAbsoluteChannelError':float(rim_delta.mean())}
for name,a in arrays.items():
    Image.fromarray(a).crop(tuple(regions['head'])).resize((306,354),Image.Resampling.NEAREST).save(run/f'material-ab-{name}-head.png')
(run/'material-ab-comparison.json').write_text(json.dumps(report,indent=2)+'\n');print(json.dumps(report['comparisons'],indent=2));print('currentVsOneExact',report['currentVsOneExact'])
