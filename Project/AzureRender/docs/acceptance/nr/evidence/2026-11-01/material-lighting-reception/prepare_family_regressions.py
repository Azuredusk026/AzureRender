import json
from pathlib import Path
run=Path(__file__).resolve().parent
out=run/'shader-family-revision'
out.mkdir(exist_ok=True)
for name in ['public_gpu','related_gpu','capture_native','capture_route','compare_compatibility','compare_body']:
    text=(run/(name+'.py')).read_text()
    for fragment in ["run/'public-gpu'","run/'related-gpu'","run/'native-captures'", "run/f'native-captures/", "run/('route-'+args.config)"]:
        text=text.replace(fragment,fragment.replace('run/',"run/'shader-family-revision'/",1))
    for fragment in ["run/('public-'+", "run/('public-gpu-'+", "run/('related-'+", "run/'native-capture-summary.json'", "run/('native-capture-summary'", "run/'compatibility-comparison.json'", "run/f'body-comparison-aa"]:
        text=text.replace(fragment,fragment.replace('run/',"run/'shader-family-revision'/",1))
    if name=='capture_route':text=text.replace("'--name',out.name", "'--name',str(out.relative_to(run))")
    (run/('family_'+name+'.py')).write_text(text,newline='\n')
spec=json.loads((run/'route-spec.json').read_text())
print('Prepared family regression drivers; frozen route source count',len(spec['frozenSourceFiles']))
