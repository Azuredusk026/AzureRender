import hashlib,json
from pathlib import Path
import numpy as np
import cv2
from PIL import Image,ImageDraw,ImageFont
root=Path.cwd();run=Path(__file__).resolve().parent;captures=run/'native-final-captures'
prior=root/'build/runs/20261031-231334-receiver-directional-light-911ab7/character-capture'
sha=lambda p:hashlib.sha256(p.read_bytes()).hexdigest()
def read(path):return np.array(Image.open(path).convert('RGB'))
def linear(a):
    a=a.astype(float)/255
    return np.where(a<=.04045,a/12.92,((a+.055)/1.055)**2.4)
regions=json.loads((root/'build/runs/20261101-014745-joint-display-domain-diagnosis-8779a0/component-comparison.json').read_text())['pairs'][0]['regions']
report={'status':'measured','visualAcceptance':'pending-five-defects','scope':'Interior rectangles measure linear-display values. They are diagnostics, not final material visibility masks. Isolated pair shares zero fog, zero SSR and no grading; exposure differs by exactly two EV.','comparisons':[]}
def compare(name,a_path,b_path,limit=None):
    a,b=read(a_path),read(b_path);al,bl=linear(a),linear(b);rows=[]
    for region in regions:
        x0,y0,x1,y1=region['bounds'];aa,bb=al[y0:y1,x0:x1],bl[y0:y1,x0:x1]
        ya=aa@np.array([.2126,.7152,.0722]);yb=bb@np.array([.2126,.7152,.0722]);med=float(np.median(ya));other=float(np.median(yb))
        rows.append({'region':region['region'],'bounds':region['bounds'],'referenceMedianLinearDisplay':med,'candidateMedianLinearDisplay':other,
            'medianLuminanceRelativeError':abs(other-med)/med if med>1e-6 else None,'maximumLinearError':float(np.abs(aa-bb).max()),
            'meanAbsoluteChannelError':float(np.abs(a[y0:y1,x0:x1].astype(float)-b[y0:y1,x0:x1]).mean()),'exactPixelFraction':float(np.all(aa==bb,axis=2).mean())})
    row={'name':name,'reference':str(a_path),'candidate':str(b_path),'referenceSha256':sha(a_path),'candidateSha256':sha(b_path),'regions':rows}
    if limit is not None:
        row['maximumLinearErrorBudget']=limit
        row['budgetScope']='Normalized character interiors only; unscaled environment has two EV of intentional exposure difference.'
        row['passed']=all(x['maximumLinearError']<=limit for x in rows if not x['region'].startswith('environment-'))
    report['comparisons'].append(row);return row
source=root/'build/runs/20261101-021345-material-radiance-normalization-1e6231/native-captures/near-source-isolated/frames/frame_000000.png';normalized=captures/'near-linear-isolated/frames/frame_000000.png'
compare('isolated-exposure-equivalence',source,normalized,.008)
compare('world-grade-versus-isolated-source',source,captures/'near-world-linear/frames/frame_000000.png')
hidden=[]
for index in range(12):
    a=prior/'hidden-reference-light/frames'/f'frame_{index:06}.png';b=captures/'full-world-hidden/frames'/f'frame_{index:06}.png'
    assert a.exists() and b.exists();aa,bb=read(a),read(b);hidden.append({'frame':index,'referenceSha256':sha(a),'candidateSha256':sha(b),'exact':bool(np.array_equal(aa,bb)),
        'differentPixels':int(np.any(aa!=bb,axis=2).sum()),'maximumChannelError':int(np.abs(aa.astype(int)-bb.astype(int)).max())})
report['hiddenEnvironmentFrames']=hidden;report['hiddenEnvironmentExact']=all(x['exact'] for x in hidden)
default_reference=prior/'near-control/frames/frame_000000.png';default_candidate=captures/'near-default-compatibility/frames/frame_000000.png'
report['defaultNativeExact']=bool(np.array_equal(read(default_reference),read(default_candidate)))
report['defaultNativeReferenceSha256']=sha(default_reference);report['defaultNativeCandidateSha256']=sha(default_candidate)
reference=read(source).astype(float)/255;candidate=read(captures/'near-world-linear/frames/frame_000000.png').astype(float)/255
report['ssimMethod']={'domain':'sRGB luminance','window':11,'sigma':1.5,'c1':.01**2,'c2':.03**2,'borderExcluded':5,'minimum':.95}
for row in report['comparisons'][1]['regions'][:5]:
    x,y,u,v=row['bounds'];a=reference[y:v,x:u]@np.array([.2126,.7152,.0722]);b=candidate[y:v,x:u]@np.array([.2126,.7152,.0722])
    blur=lambda q:cv2.GaussianBlur(q,(11,11),1.5)
    ma,mb=blur(a),blur(b);va,vb,cov=blur(a*a)-ma*ma,blur(b*b)-mb*mb,blur(a*b)-ma*mb
    values=((2*ma*mb+.01**2)*(2*cov+.03**2))/((ma*ma+mb*mb+.01**2)*(va+vb+.03**2))
    row['staticLuminanceSsim']=float(values[5:-5,5:-5].mean())
report['characterInteriorMedianLuminancePassed']=all(row['medianLuminanceRelativeError']<=.10 for row in report['comparisons'][1]['regions'][:5])
report['characterInteriorStaticSsimPassed']=all(row['staticLuminanceSsim']>=.95 for row in report['comparisons'][1]['regions'][:5])
canvas=Image.new('RGB',(3*600,640),(20,24,28));draw=ImageDraw.Draw(canvas)
for i,(title,path) in enumerate([('原联合配置',prior/'near-control/frames/frame_000000.png'),('同机位独立显示参照',source),('联合显示策略候选',captures/'near-world-linear/frames/frame_000000.png')]):
    image=Image.open(path).convert('RGB').crop((340,260,1190,1110)).resize((600,600),Image.Resampling.LANCZOS)
    canvas.paste(image,(i*600,35));draw.text((i*600+12,5),title,fill='white',font=ImageFont.truetype('C:/Windows/Fonts/msyh.ttc',21))
canvas.save(run/'native-final-comparison.png')
(run/'native-final-comparison.json').write_text(json.dumps(report,indent=2)+'\n')
print(json.dumps({'isolatedPassed':report['comparisons'][0]['passed'],'hiddenEnvironmentExact':report['hiddenEnvironmentExact'],'worldRegionErrors':[(x['region'],x['medianLuminanceRelativeError']) for x in report['comparisons'][1]['regions']]}))
assert report['comparisons'][0]['passed'],'Native material normalization fails independent exposure control'
assert report['hiddenEnvironmentExact'],'Unscaled environment changed'
assert report['defaultNativeExact'],'Default native material output changed'

assert report['characterInteriorMedianLuminancePassed'],'Native character interiors exceed ten percent luminance error'
assert report['characterInteriorStaticSsimPassed'],'Native character interiors fall below 0.95 static SSIM'
