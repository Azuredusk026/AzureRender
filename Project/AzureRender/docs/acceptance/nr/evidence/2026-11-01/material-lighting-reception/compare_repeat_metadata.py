import json
from pathlib import Path
run=Path(__file__).resolve().parent;rows=[]
def compare(a,b,path):
 if type(a)!=type(b):rows.append(dict(path=path,kind='type'));return
 if isinstance(a,dict):
  for k in sorted(set(a)|set(b)):
   if k not in a or k not in b:rows.append(dict(path=path+'/'+k,kind='missing'))
   else:compare(a[k],b[k],path+'/'+k)
 elif isinstance(a,list):
  if len(a)!=len(b):rows.append(dict(path=path,kind='length',before=len(a),after=len(b)))
  else:
   for i,(x,y) in enumerate(zip(a,b)):compare(x,y,path+'/'+str(i))
 elif a!=b:rows.append(dict(path=path,kind='value',before=a if isinstance(a,(int,float,bool)) else str(a)[:100],after=b if isinstance(b,(int,float,bool)) else str(b)[:100]))
for name in ['runtime.json','frames/capture_manifest.json']:
 a=json.loads((run/'native-captures/full-world-hidden'/name).read_text());b=json.loads((run/'compatibility-repeat/full-world-hidden'/name).read_text());compare(a,b,name)
(run/'repeat-metadata-comparison.json').write_bytes((json.dumps(dict(status='measured',differences=rows),indent=2)+'\n').encode());print('Metadata differences',len(rows));print(json.dumps(rows[:24]))
