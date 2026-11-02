import argparse,hashlib,importlib.util,json,os,sys
from pathlib import Path
run=Path(__file__).resolve().parent;root=run.parents[2]
p=argparse.ArgumentParser();p.add_argument('--expect',choices=['red','green'],required=True);p.add_argument('--configuration',choices=['release','debug'],default='release');args=p.parse_args()
out=run/'shader-family-revision'/('directory-boundary-'+args.configuration+'-'+args.expect)
out.mkdir(exist_ok=False)
program=root/('build/ninja-msvc-'+args.configuration)/'AzurePlayer.exe'
shaderRoot=out/'resource_integral_directory';(shaderRoot/'shaders').mkdir(parents=True)
sha=lambda p:hashlib.sha256(p.read_bytes()).hexdigest()
inputs={}
for source in sorted((program.parent/'shaders').glob('*.spv')):
    os.link(source,shaderRoot/'shaders'/source.name);inputs[source.name]=sha(source)
sys.path.insert(0,str(root/'tools'))
spec=importlib.util.spec_from_file_location('material_lighting_boundary',root/'tools/test_material_lighting_gpu.py')
module=importlib.util.module_from_spec(spec);spec.loader.exec_module(module)
previous=os.environ.get('AZURERENDER_RESOURCE_ROOT');os.environ['AZURERENDER_RESOURCE_ROOT']=str(shaderRoot)
failure=None
try:
    module.run(program,out/'captures',['generic-composed','fabric-composed'],['indexed','fixed'],
        'enabled' if args.configuration=='debug' else 'disabled')
except Exception as error:failure=repr(error)
finally:
    if previous is None:os.environ.pop('AZURERENDER_RESOURCE_ROOT',None)
    else:os.environ['AZURERENDER_RESOURCE_ROOT']=previous
logs=[p.read_text(errors='replace') for p in (out/'captures').rglob('player.log')]
if args.expect=='red':
    assert failure and any('Explicit material lighting pipeline is unavailable' in text for text in logs),failure
else:
    assert failure is None,failure
    summary=json.loads((out/'captures/summary.json').read_text())
    assert summary['status']=='passed' and len(summary['cases'])==4
report=dict(status='red-confirmed' if args.expect=='red' else 'passed',configuration=args.configuration,
    programSha256=sha(program),shaders=inputs,resourceRoot=str(shaderRoot),expectedFailure=failure,
    scope='Only the shader parent directory contains the integral family token; material programs are ordinary generic and fabric.')
(out/'receipt.json').write_text(json.dumps(report,indent=2)+'\n')
print(json.dumps({k:v for k,v in report.items() if k!='shaders'}))
