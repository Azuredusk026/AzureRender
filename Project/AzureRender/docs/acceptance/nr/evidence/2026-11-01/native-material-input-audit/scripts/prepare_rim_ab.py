import copy,json,uuid
from pathlib import Path
run=Path(__file__).resolve().parent;project=run/'material-ab'
source=project/'profiles/current-8.azurematerial';doc=json.loads(source.read_text())
doc['surface']['materials'][0]['extras']['azureRenderMaterial'].pop('styleParameters')
target=project/'profiles/current-8-default-rim.azurematerial';target.write_text(json.dumps(doc,indent=2)+'\n')
meta=json.loads(Path(str(source)+'.azmeta').read_text());meta['id']=str(uuid.uuid5(uuid.NAMESPACE_URL,'native-material-ab:8-default-rim'));Path(str(target)+'.azmeta').write_text(json.dumps(meta)+'\n')
level=json.loads((project/'levels/current-head.azurelevel').read_text());hero=next(n for n in level['nodes'] if n['id']=='hero');hero['materialSlots']['name:M_actor_laevat_face_01']=meta['id']
name='current-head-default-rim';(project/f'levels/{name}.azurelevel').write_text(json.dumps(level,indent=2)+'\n')
descriptor=json.loads((project/'current-head.azureproject').read_text());descriptor['id']=name;descriptor['startupScene']=f'levels:/{name}.azurelevel';path=project/(name+'.azureproject');path.write_text(json.dumps(descriptor,indent=2)+'\n')
(run/'material-ab-extra-cases.json').write_text(json.dumps([{'name':name,'project':str(path),'captureFrames':32}],indent=2)+'\n')
