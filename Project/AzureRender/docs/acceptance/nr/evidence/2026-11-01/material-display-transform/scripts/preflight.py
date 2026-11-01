import json,subprocess,sys
from pathlib import Path
root=Path.cwd();run=Path(__file__).resolve().parent;sys.path.insert(0,str(root/'tools'))
from test_material_display_gpu import prepare,FAMILIES
folder=run/'fixture-preflight';folder.mkdir();rows=[]
for family in FAMILIES:
    for mode in ('reference','scene','linear'):
        name=family+'-'+mode;definition=prepare(folder/name,family,mode)
        result=subprocess.run([str(root/'build/ninja-msvc-release/AzurePlayer.exe'),'--project',str(definition['project']),'--check-project'],capture_output=True)
        (folder/(name+'.log')).write_bytes(result.stdout+result.stderr)
        rows.append(dict(name=name,exitCode=result.returncode))
(folder/'summary.json').write_text(json.dumps(rows,indent=2)+'\n')
print(json.dumps(dict(cases=len(rows),failures=[r for r in rows if r['exitCode']])),flush=True)
