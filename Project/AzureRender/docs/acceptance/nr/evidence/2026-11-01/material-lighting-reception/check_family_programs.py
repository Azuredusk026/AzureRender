import argparse,hashlib,json
from pathlib import Path
run=Path(__file__).resolve().parent;root=run.parents[2]
p=argparse.ArgumentParser();p.add_argument('--configuration',choices=['release','debug'],required=True);args=p.parse_args()
old=json.loads((root/'build/runs/20261101-043757-material-display-transform-93d6e0/release-final-build.json').read_text())['shaders']
current=json.loads((run/(args.configuration+'-build.json')).read_text())
assert current['status']=='built-contract-tests-passed'
changed=[]
for name,digest in old.items():
    if current['shaders'].get(name)!=digest:changed.append(name)
new=set(current['shaders'])-set(old)
assert new and all(name.endswith('_lighting.frag.spv') for name in new),new
report=dict(status='passed' if not changed else 'failed',configuration=args.configuration,referencePrograms=len(old),isolatedLightingPrograms=len(new),changedDefaultPrograms=changed,sourceFiles=current['sourceFiles'])
(run/'shader-family-revision'/(args.configuration+'-program-compatibility.json')).write_text(json.dumps(report,indent=2)+'\n')
print(json.dumps({k:v for k,v in report.items() if k!='sourceFiles'}))
assert not changed,changed
