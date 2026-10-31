import json,subprocess,sys
from pathlib import Path
root=Path.cwd();run=Path(__file__).resolve().parent;sys.path.insert(0,str(root/'tools'))
from native_render.light_lab import generate
folder=run/'boundary-red-2';generate(folder,'dark');path=folder/'plane.gltf';model=json.loads(path.read_text())
model['materials'][0]['extras']={'azureRenderMaterial':{'schemaVersion':2,'class':'generic','features':[],'radianceScale':16.0000001}}
path.write_text(json.dumps(model));command=[str(root/'build/ninja-msvc-release/AzurePlayer.exe'),'--project',str(folder/'lab.azureproject'),'--check-project']
result=subprocess.run(command,capture_output=True);(run/'boundary-red-2.log').write_bytes(result.stdout+result.stderr)
report={'expected':'reject-raw-json-value-above-sixteen','input':16.0000001,'exitCode':result.returncode,'failedToReject':result.returncode==0,'command':command}
(run/'boundary-red-2.json').write_text(json.dumps(report,indent=2)+'\n');print(json.dumps(report));assert result.returncode!=0,'Out-of-range JSON rounded into the legal float boundary'
