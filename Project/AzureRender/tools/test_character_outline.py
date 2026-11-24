"""Measure silhouette expansion at two output sizes and three camera distances."""
import argparse,json,sys
from pathlib import Path
import numpy as np
from PIL import Image
sys.path.insert(0,str(Path(__file__).resolve().parents[1]/'build/qa-python'))
import cv2
from test_character_finish import capture

def main():
    p=argparse.ArgumentParser(__doc__)
    for name in ['executable','asset','output']:p.add_argument('--'+name,type=Path,required=True)
    args=p.parse_args();root=args.output.resolve();root.mkdir(parents=True,exist_ok=True);cases=[]
    for height in [720,1440]:
        for camera in ['full-body-front','face-front','back-detail']:
            flags=['--width',str(height*16//9),'--height',str(height),'--anti-aliasing','supersample']
            frames=[]
            for state in ['enabled','disabled']:
                folder=capture(args.executable.resolve(),args.asset.resolve(),root,f'{camera}-{height}-{state}',camera,'neutral-material',
                               extra=flags+['--qa-effect','outline','--qa-effect-state',state])
                frames.append(np.asarray(Image.open(folder/'frame_000000.png').convert('RGB'),dtype=np.int16))
            enabled,disabled=frames
            # QA background is a row-constant gradient. Measure only new pixels
            # outside the unoutlined silhouette, excluding the bottom platform.
            surface=np.max(np.abs(disabled-disabled[:,0:1]),axis=2)>12
            surface=cv2.morphologyEx(surface.astype(np.uint8),cv2.MORPH_CLOSE,np.ones((3,3),np.uint8))>0
            added=(np.max(np.abs(enabled-disabled),axis=2)>4)&~surface
            added[int(height*.9):]=False
            distance=cv2.distanceTransform((~surface).astype(np.uint8),cv2.DIST_L2,cv2.DIST_MASK_PRECISE)[added]
            assert len(distance)>100,'Silhouette outline must contribute visible pixels'
            result={'camera':camera,'height':height,'outlinePixels':.8,'changedExteriorPixels':len(distance),
                    'exteriorDistanceP95Pixels':float(np.percentile(distance,95)),
                    'exteriorDistanceP99Pixels':float(np.percentile(distance,99))}
            assert result['exteriorDistanceP99Pixels']<=2.25,result
            cases.append(result);print(json.dumps(result),flush=True)
    (root/'summary.json').write_text(json.dumps({'passed':True,'cases':cases},indent=2),encoding='utf-8')
if __name__=='__main__':main()
