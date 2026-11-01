import json
from pathlib import Path
import numpy as np
run=Path(__file__).resolve().parent;source=json.loads((run/'native-bindings.json').read_text());rows=[]
for draw in source['draws']:
 c=next(c for c in draw['constantBuffers'] if c['index']==1);v=np.fromfile(run/'native-bindings'/c['path'],dtype='<f4').reshape(-1,4);rows.append(dict(event=draw['event'],globalRows={str(i):v[i].tolist() for i in [27,29,110,111,115,116,117,121,122,123,140]}))
(run/'native-npr-constant-rows.json').write_bytes((json.dumps(dict(status='captured-constant-rows',draws=rows),indent=2)+'\n').encode())
for row in rows:print(row['event'],row['globalRows']['110'],row['globalRows']['111'],row['globalRows']['140'])
