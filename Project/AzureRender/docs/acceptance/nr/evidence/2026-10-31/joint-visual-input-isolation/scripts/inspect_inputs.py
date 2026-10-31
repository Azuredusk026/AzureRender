import json
from pathlib import Path

root = Path.cwd()
run = Path(__file__).resolve().parent
sources = {
    'portrait': root/'build/runs/20261031-135416-character-portrait-recovery-0773ca/original-reference/project/high.azureproject',
    'near': root/'build/runs/20261031-172112-hair-source-inputs-14d199/project/near-restored.azureproject',
    'joint': root/'build/runs/20261031-172112-hair-source-inputs-14d199/project/full-restored.azureproject',
    'scene': root/'build/runs/20261029-045743-snapshot-shading-budget-8ee0b1/full-candidate/enclosure-sampler.azureproject',
}

def resolve(project, document, uri):
    mount, rel = uri.split(':/', 1)
    return project.parent/next(x['path'] for x in document['mounts'] if x['name']==mount)/rel

result = {}
for name, project in sources.items():
    doc=json.loads(project.read_text(encoding='utf-8-sig'))
    level=resolve(project,doc,doc['startupScene'])
    scene=json.loads(level.read_text(encoding='utf-8-sig'))
    result[name]={'project':str(project),'level':str(level),'keys':list(scene),
        'renderSettings':scene.get('renderSettings'),
        'lightsAndCamera':[n for n in scene.get('nodes',[]) if any('light' in str(c).lower() or 'camera' in str(c).lower() for c in n.get('components',{}))]}
(run/'input-settings.json').write_text(json.dumps(result,indent=2,ensure_ascii=False)+'\n',encoding='utf-8')
for name,row in result.items():
    print(name, row['level'])
    print(json.dumps(row['renderSettings'],ensure_ascii=False))
    print('lights/camera',len(row['lightsAndCamera']))
