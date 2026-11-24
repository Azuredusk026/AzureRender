import json
from pathlib import Path
run=Path(__file__).resolve().parent;read=lambda p:json.loads(p.read_text());report=dict(status='partial',visualAcceptance='failed-user-review',publicLighting=[],related=[],routes=[])
for cfg in ['release','debug']:
 public=read(run/'public-gpu'/cfg/'summary.json');assert public['status']=='passed';report['publicLighting'].append(dict(configuration=cfg,captures=len(public['cases'])))
 related=read(run/('related-'+cfg+'.json'));assert related['status']=='passed' and len(related['checks'])==7;frames=0;instances=0
 for item in related['checks']:
  audit=read(run/'related-gpu'/cfg/(item['name']+'-audit.json'));assert audit['status']=='passed'
  for player in audit['players']:
   assert player['released'];cmd=player['command'];frames+=int(cmd[cmd.index('--capture-frames')+1]);instances+=1
 report['related'].append(dict(configuration=cfg,suites=7,frames=frames,successfulPlayerInstances=instances))
 p=run/('route-'+cfg)/'summary.json'
 if p.exists():
  route=read(p);assert route['status']=='behavior-passed-visual-review-pending';assert read(p.parent/'behavior-verification.json')['status']=='passed';report['routes'].append(dict(configuration=cfg,frames=540,actions=13,status='passed'))
report['nativeFrames']=sum(len(c['images']) for c in read(run/'native-capture-summary.json')['cases']);report['strictEnvironmentCompatibility']='failed';(run/'completed-regression-summary.json').write_bytes((json.dumps(report,indent=2)+'\n').encode());print(json.dumps(report))
