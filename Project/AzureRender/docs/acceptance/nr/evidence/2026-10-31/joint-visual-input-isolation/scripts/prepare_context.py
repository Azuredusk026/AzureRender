import copy,hashlib,json
from pathlib import Path
root=Path.cwd();run=Path(__file__).resolve().parent;project=run/'project'
scene=json.loads((project/'scene/full-npr-shadow-override.azurelevel').read_text())
before=copy.deepcopy(scene)
keep={'hero','camera'}|{x['nodeId'] for x in scene['lights']}
byid={n['id']:n for n in scene['nodes']}
while True:
    parents={byid[k].get('parentId') for k in keep if k in byid}-{None,''}
    if parents<=keep:break
    keep|=parents
scene['nodes']=[n for n in scene['nodes'] if n['id'] in keep]
scene['resources']=[r for r in scene['resources'] if r['id']=='hero']
scene['sceneImports']=[]
assert len(scene['resources'])==1 and len([n for n in scene['nodes'] if n.get('resourceId')])==1
assert scene['lights']==before['lights'] and scene['renderSettings']==before['renderSettings']
name='same-input-character-only';level=project/'scene'/(name+'.azurelevel')
level.write_text(json.dumps(scene,indent=2)+'\n')
descriptor=json.loads((project/'control.azureproject').read_text());descriptor['id']=name;descriptor['startupScene']='stage:/'+level.name
path=project/(name+'.azureproject');path.write_text(json.dumps(descriptor,indent=2)+'\n')
(run/'context-cases.json').write_text(json.dumps([{'name':name,'project':str(path),'levelSha256':hashlib.sha256(level.read_bytes()).hexdigest()}],indent=2)+'\n')
(run/'context-delta.json').write_text(json.dumps({'changedFields':['nodes','resources','sceneImports'],'nodes':len(scene['nodes']),'resources':len(scene['resources']),'retained':['same character asset and pose','same camera','same lights','same materials','same NPR composition','same GI and probe resources','same post processing'],'scope':'Character material context isolation. Scene cascade casters and SSR sources differ; main visibility override is 1 in both, and those differences remain out of scope for general equality.'},indent=2)+'\n')
(run/'capture_context.py').write_text((run/'capture.py').read_text().replace("run/'cases.json'","run/'context-cases.json'").replace("run/'summary.json'","run/'context-summary.json'"))
print('Prepared character-only context with',len(scene['nodes']),'nodes')
