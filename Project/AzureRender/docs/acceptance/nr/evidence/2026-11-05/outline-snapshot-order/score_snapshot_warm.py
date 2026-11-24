from pathlib import Path
import json,numpy as np,cv2
from PIL import Image
cv2.setNumThreads(1)
r=Path(__file__).resolve().parent;ref=np.asarray(Image.open(r/'combined-static-reference.png').convert('RGB'));img=np.asarray(Image.open(r/'snapshot-shell-warm-capture/frames/frame_000000.png').convert('RGB'));folder=r.parent/'20261023-140322-skin-ramp-capture-validation-4e1751'
v=lambda im:im.astype(float)/255
l=lambda im:np.where(v(im)<=.04045,v(im)/12.92,((v(im)+.055)/1.055)**2.4)@np.array([.2126,.7152,.0722])
x=v(ref)@np.array([.2126,.7152,.0722]);y=v(img)@np.array([.2126,.7152,.0722]);g=lambda a:cv2.GaussianBlur(a,(11,11),1.5);a=g(x);b=g(y);s=((2*a*b+.01**2)*(2*(g(x*y)-a*b)+.03**2))/((a*a+b*b+.01**2)*(g(x*x)-a*a+g(y*y)-b*b+.03**2));report={}
for k in ('skin','face'):
 m=cv2.erode(np.asarray(Image.open(folder/(k+'-visible-mask.png')).convert('L')),np.ones((13,13),'uint8'))>0
 delta=abs(ref.astype('int16')-img);lum=l(img)[m];orig=l(ref)[m];report[k]=dict(pixels=int(m.sum()),ssim=float(s[m].mean()),medianLuminanceRelativeError=float(abs(np.median(lum)-np.median(orig))/np.median(orig)),medianRgbError=float(np.median(delta[m])),maxRgbError=int(delta[m].max()))
print(report);(r/'snapshot-shell-warm-skin-face.json').write_text(json.dumps(report,indent=2))
