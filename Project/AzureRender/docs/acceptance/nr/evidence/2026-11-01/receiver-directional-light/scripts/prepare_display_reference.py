import copy,hashlib,json
from pathlib import Path
root=Path.cwd();run=Path(__file__).resolve().parent;project=run/'project'
level=json.loads((project/'scene/near-reference-light.azurelevel').read_text())
source=root/'build/runs/20261031-135416-character-portrait-recovery-0773ca/original-reference/project/high/reference.azurelevel'
reference=json.loads(source.read_text())
level['renderSettings']['nprLighting']['outdoorGain']/=.25
for light in level['lights']:
    if light['id'].startswith('portrait-reference-') and light['shape']['type']!='directional':light['intensity']/=.25
for key in ['exposure','toneMapping','gradeLut','bloomEnabled']:
    level['renderSettings'][key]=copy.deepcopy(reference['renderSettings'][key])
name='near-reference-source-display';path=project/'scene'/(name+'.azurelevel');path.write_text(json.dumps(level,indent=2)+'\n')
descriptor=json.loads((project/'near-reference-light.azureproject').read_text());descriptor['id']=name;descriptor['startupScene']='stage:/'+path.name
dest=project/(name+'.azureproject');dest.write_text(json.dumps(descriptor,indent=2)+'\n')
rows=json.loads((run/'character-cases.json').read_text());assert not any(x['name']==name for x in rows)
rows.append({'name':name,'project':str(dest),'levelSha256':hashlib.sha256(path.read_bytes()).hexdigest()})
(run/'character-cases.json').write_text(json.dumps(rows,indent=2)+'\n')
print('Prepared same-pose reference display; native character radiance restored before exposure -2')
