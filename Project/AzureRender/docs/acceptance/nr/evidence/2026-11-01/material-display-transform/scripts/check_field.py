import argparse,json,subprocess,sys
from pathlib import Path
root=Path.cwd();run=Path(__file__).resolve().parent;sys.path.insert(0,str(root/'tools'))
from test_material_radiance_gpu import prepare
parser=argparse.ArgumentParser();parser.add_argument('--name',required=True);parser.add_argument('--prepare',action='store_true');args=parser.parse_args()
folder=run/'field-fixture'
if args.prepare:
    definition=prepare(folder,'unlit',.25,0);model_path=folder/'plane.gltf';model=json.loads(model_path.read_text())
    for material in model['materials']:
        if material['name']=='radiance-subject':material['extras']['azureRenderMaterial']['displayTransform']='linear'
    model_path.write_text(json.dumps(model,indent=2)+'\n')
command=[str(root/'build/ninja-msvc-release/AzurePlayer.exe'),'--project',str(folder/'lab.azureproject'),'--check-project']
result=subprocess.run(command,capture_output=True);log=result.stdout+result.stderr;(run/(args.name+'.log')).write_bytes(log)
report={'expected':'valid-version-two-linear-display-profile-accepted','exitCode':result.returncode,'command':command,'field':'displayTransform',
    'accepted':result.returncode==0,'unknownFieldDiagnostic':b'unknown field displayTransform' in log}
(run/(args.name+'.json')).write_text(json.dumps(report,indent=2)+'\n');print(json.dumps(report))
assert result.returncode==0,'Valid displayTransform profile rejected: '+log.decode('utf-8',errors='replace')[-350:]
