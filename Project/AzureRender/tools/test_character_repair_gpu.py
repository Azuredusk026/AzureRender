"""Freeze visual failure gates before repairing the renderer."""
import argparse
import json
import subprocess
from pathlib import Path
import numpy as np
from PIL import Image

def main():
    parser = argparse.ArgumentParser(__doc__)
    parser.add_argument('--executable', type=Path, required=True)
    parser.add_argument('--asset', type=Path, required=True)
    parser.add_argument('--output', type=Path, required=True)
    args = parser.parse_args()
    args.output.mkdir(parents=True, exist_ok=True)
    results = {}
    for name, flags in [('base', []), ('off', ['--qa-effect','overlay','--qa-effect-state','disabled'])]:
        target = args.output / name
        cmd = [str(args.executable.resolve()),'--asset',str(args.asset.resolve()),'--width','1280','--height','720',
               '--qa-camera','face-front','--capture-frames','1','--capture-dir',str(target.resolve()),*flags]
        run = subprocess.run(cmd,capture_output=True,text=True,encoding='utf-8',errors='replace')
        (args.output/(name+'.log')).write_text(run.stdout+run.stderr,encoding='utf-8')
        if run.returncode: raise RuntimeError(run.stderr)
        results[name] = np.asarray(Image.open(target/'frame_000000.png').convert('RGB'),dtype=np.int16)
    # White Face-D islands protruding in front of red bangs. Fixed forehead ROI.
    y0,y1,x0,x1 = 245,395,430,850
    a = results['base'][y0:y1,x0:x1]; b = results['off'][y0:y1,x0:x1]
    white_leak = (a.min(axis=2)>185) & ((a.max(axis=2)-a.min(axis=2))<35) & (b[:,:,0]>b[:,:,1]*1.5)
    count = int(white_leak.sum())
    report = {'foreheadWhiteLeakPixels':count,'maximum':16,'roi':[x0,y0,x1,y1]}
    (args.output/'summary.json').write_text(json.dumps(report,indent=2),encoding='utf-8')
    print(json.dumps(report))
    assert count <= 16, 'Face-D overlay leaks in front of the bangs'

if __name__ == '__main__': main()
