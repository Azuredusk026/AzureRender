from pathlib import Path
import json,hashlib,numpy as np
from PIL import Image
root=Path.cwd();run=Path(__file__).resolve().parent;current=root/'build/runs/20261031-135416-character-portrait-recovery-0773ca/original-reference';old=root/'build/runs/20261024-074605-native-material-aa-reference-9eb92d';mask_file=root/'build/runs/20261023-105023-character-composite-contribution-diagnosis-acb8e5/native-hair-visible-mask.png'
def read(path):
 a=np.asarray(Image.open(path).convert('RGB')).astype(np.float32)/255
 return np.where(a<=.04045,a/12.92,((a+.055)/1.055)**2.4)
high=read(current/'high/frames/frame_000000.png').reshape(720,4,1280,4,3).mean((1,3));hair=np.asarray(Image.open(mask_file).convert('L')).reshape(720,2,1280,2).max((1,3))>0;padded=np.pad(hair,1);interior=np.logical_and.reduce([padded[y:y+720,x:x+1280] for y in range(3) for x in range(3)]);boundary=hair&~interior
report={'status':'characterized','scope':'Beauty-only linear-display AA diagnostics against the same actual 4x reference. Fixed historical masks and regions; native HDR and full visual gates remain separate.','baseCommit':'ca9a217','maskSha256':hashlib.sha256(mask_file.read_bytes()).hexdigest(),'regions':[]}
sets={name:np.stack([read(base/'temporal/frames'/('frame_%06d.png'%i)) for i in range(24,32)]) for name,base in [('current',current),('historical',old)]}
for name,box in [('crown',(607,80,670,121)),('hair-interior',(570,65,715,220)),('fine-hair-boundary',(570,65,715,220)),('clothing',(500,220,800,550))]:
 x0,y0,x1,y1=box;ref=high[y0:y1,x0:x1];mask=(np.abs(ref-high[0,0]).max(2)>.005) if name=='clothing' else (boundary if name=='fine-hair-boundary' else interior)[y0:y1,x0:x1].copy();expected=ref[mask];energy=float(expected.sum());row={'name':name,'boxXYXY':box,'pixels':int(mask.sum()),'referenceEnergy':energy}
 for label,frames in sets.items():
  values=frames[:,y0:y1,x0:x1][:,mask];row[label]={'lastEightEnergyRatios':[float(v.sum()/energy) for v in values],'meanAbsoluteError':float(np.abs(values-expected).mean()),'temporalMeanDeviation':float(values.std(0).mean()),'temporalMaximumRange':float(np.ptp(values,axis=0).max())}
 row['currentToHistoricalErrorRatio']=row['current']['meanAbsoluteError']/max(row['historical']['meanAbsoluteError'],1e-12);row['currentToHistoricalTemporalDeviationRatio']=row['current']['temporalMeanDeviation']/max(row['historical']['temporalMeanDeviation'],1e-12);report['regions'].append(row)
(run/'comparison.json').write_text(json.dumps(report,indent=2)+'\n');print(json.dumps(report,indent=2))
