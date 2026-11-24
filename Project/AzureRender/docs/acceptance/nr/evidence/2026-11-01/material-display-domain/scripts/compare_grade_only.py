import hashlib,json
from pathlib import Path
import numpy as np
from PIL import Image
root=Path.cwd();run=Path(__file__).resolve().parent;source_run=root/'build/runs/20261101-021345-material-radiance-normalization-1e6231'
paths={'control':source_run/'native-captures/near-normalized-isolated/frames/frame_000000.png',
    'gradeOnly':run/'grade-only-capture/frames/frame_000000.png','world':source_run/'native-captures/near-world-normalized/frames/frame_000000.png'}
images={key:np.array(Image.open(path).convert('RGB')) for key,path in paths.items()}
def linear(v):
    v=v.astype(float)/255
    return np.where(v<=.04045,v/12.92,((v+.055)/1.055)**2.4)
regions=json.loads((root/'build/runs/20261101-014745-joint-display-domain-diagnosis-8779a0/component-comparison.json').read_text())['pairs'][0]['regions'][:5]
rows=[]
for region in regions:
    x,y,z,w=region['bounds'];colors={key:linear(image[y:w,x:z]) for key,image in images.items()};luminances={key:float(np.median(value@np.array([.2126,.7152,.0722]))) for key,value in colors.items()}
    rows.append({'region':region['region'],'bounds':region['bounds'],'medianLinearDisplay':luminances,
        'gradeOnlyRelativeLuminanceChange':luminances['gradeOnly']/luminances['control']-1,
        'remainingWorldRelativeLuminanceChange':luminances['world']/luminances['gradeOnly']-1,
        'gradeOnlyMaximumChannelDifference':int(np.abs(images['gradeOnly'][y:w,x:z].astype(int)-images['control'][y:w,x:z].astype(int)).max())})
report={'status':'measured','onlyRenderDifference':'renderSettings.gradeLut','scope':'Actual production Player with identical normalized material documents, pose, camera, light rig and render settings. Fog and SSR remain disabled and bloom remains off in both isolated captures. The full-world image provides a separate postprocess comparison.',
    'inputs':{key:{'path':str(path),'sha256':hashlib.sha256(path.read_bytes()).hexdigest()} for key,path in paths.items()},'regions':rows,'visualAcceptance':'failed-user-review'}
(run/'grade-only-comparison.json').write_text(json.dumps(report,indent=2)+'\n');print(json.dumps(rows))
