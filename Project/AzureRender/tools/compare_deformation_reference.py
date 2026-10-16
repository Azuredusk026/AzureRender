"""Compare independently sampled world-space deformation references."""
import argparse
import json
from pathlib import Path
import numpy as np


def compare(a, b, position_limit=.001, rotation_limit=.1):
    if a.get('space') != b.get('space') or a.get('units') != b.get('units'): raise ValueError('Reference coordinate contract mismatch')
    aa, bb = a['samples'], b['samples']
    if not aa or len(aa) != len(bb): raise ValueError('Missing reference samples')
    pos = angle = 0.
    for x,y in zip(aa,bb):
        if abs(x['time'] - y['time']) > 1e-7: raise ValueError('Sample times differ')
        for category in ('vertices', 'bones'):
            if x[category].keys() != y[category].keys(): raise ValueError('Reference identities differ')
            for key in x[category]:
                v,w = np.asarray(x[category][key]), np.asarray(y[category][key])
                if not np.isfinite(v).all() or not np.isfinite(w).all(): raise ValueError('Nonfinite reference')
                if category == 'vertices': pos = max(pos, float(np.linalg.norm(v-w)))
                else:
                    v,w = v.reshape(4,4),w.reshape(4,4)
                    pos = max(pos, float(np.linalg.norm(v[:3,3]-w[:3,3])))
                    vr = v[:3,:3] / np.linalg.norm(v[:3,:3],axis=0)
                    wr = w[:3,:3] / np.linalg.norm(w[:3,:3],axis=0)
                    angle = max(angle, float(np.degrees(np.arccos(np.clip((np.trace(vr.T@wr)-1)/2,-1,1)))))
    return {'passed': pos <= position_limit and angle <= rotation_limit, 'maximumPositionMetres': pos,
            'maximumRotationDegrees': angle, 'samples': len(aa)}


if __name__ == '__main__':
    p = argparse.ArgumentParser(__doc__); p.add_argument('reference', type=Path); p.add_argument('candidate', type=Path)
    args=p.parse_args(); result=compare(json.loads(args.reference.read_text()),json.loads(args.candidate.read_text()))
    print(json.dumps(result,indent=2)); raise SystemExit(0 if result['passed'] else 1)
