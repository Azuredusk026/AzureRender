import hashlib,json
from pathlib import Path
import numpy as np
root=Path.cwd();run=Path(__file__).resolve().parent
source=root/'build/runs/20261022-212213-native-binding-restoration-d39ea8/frame10401/frame-bone-comparison.json'
native=json.loads(source.read_text());head=next(x for x in native['comparisons'] if x['bone']=='Bip001_Head')
matroot=run/'project/assets/restored-character-materials'
face=json.loads((matroot/'material-8.azurematerial').read_text())['surface']['materials'][0]['extras']['azureRenderMaterial']['faceSdf']
hair=json.loads((matroot/'material-9.azurematerial').read_text())['surface']['materials'][0]['extras']['azureRenderMaterial']['hair']['projectedRamp']['frame']
assert face['headNode']==hair['node']=='Bip001_Head' and face['headBasis']==hair['basis']
basis=np.array(face['headBasis']).reshape(3,3,order='F');flip=np.diag([-1.,1.,1.])
h=np.array(head['world']);reserved=np.array(native['reserved'])
actual=flip@h[:3,:3]@flip@basis;expected=flip@reserved[:3,:3]
actual/=np.linalg.norm(actual,axis=0);expected/=np.linalg.norm(expected,axis=0)
error=float(np.max(np.abs(actual-expected)))
report={'status':'orientation-source-consistent-position-residual-retained',
    'scope':'Current declared face/hair basis against previously captured head matrix and shared reserved material frame. Dynamic normals, tangent reconstruction and full visual appearance remain separate checks.',
    'source':str(source),'sourceSha256':hashlib.sha256(source.read_bytes()).hexdigest(),
    'node':hair['node'],'basis':hair['basis'],'maximumOrientationComponentError':error,
    'nativeOriginResidualMetres':float(np.linalg.norm(h[:3,3]-reserved[:3,3]))}
(run/'native-frame-audit.json').write_text(json.dumps(report,indent=2)+'\n')
assert error<1e-5
print(json.dumps(report,indent=2))
