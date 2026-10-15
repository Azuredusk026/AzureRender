"""Compare three seeded real-engine sample budgets against their 1024-sample reference."""
import argparse,json
from pathlib import Path
import numpy as np
from PIL import Image
from render_pathtracing_demo import run
MODELS=('DamagedHelmet','ABeautifulGame','TransmissionRoughnessTest')
def test(executable,projects,output):
    output=Path(output);results={}
    for name in MODELS:
        rows=[]
        for seed in (0,1,2):
            folder=output/name/str(seed)
            run(executable,Path(projects)/name/'project.azureproject',folder,(64,256,1024),seed,(320,240),False)
            images={n:np.asarray(Image.open(folder/str(n)/f'captures/samples-{n}.png').convert('RGB'),dtype=float)[40:210,45:275]/255 for n in (64,256,1024)}
            row={str(n):float(np.mean((images[n]-images[1024])**2)) for n in (64,256)};rows.append(row)
        mean={str(n):float(np.mean([r[str(n)] for r in rows])) for n in (64,256)}
        assert mean['256']<mean['64'],(name,mean)
        results[name]={'seeds':rows,'meanSquaredError':mean,'passed':True,'referenceSamples':1024,'size':[320,240],'region':[45,40,275,210]}
        output.mkdir(parents=True,exist_ok=True);(output/'summary.json').write_text(json.dumps(results,indent=2),encoding='utf-8')
        print(name,'convergence passed',mean,flush=True)
    return results
if __name__=='__main__':
    p=argparse.ArgumentParser(__doc__);p.add_argument('--executable',required=True);p.add_argument('--projects',required=True);p.add_argument('--output',required=True);a=p.parse_args();test(a.executable,a.projects,a.output)
