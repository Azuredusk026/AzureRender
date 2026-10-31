import json,sys
from pathlib import Path
import numpy as np
root=Path.cwd();run=Path(__file__).resolve().parent;sys.path.insert(0,str(root/'tools'))
from retarget_animation import read_glb,trs_matrix
source=root/'build/runs/20261031-135416-character-portrait-recovery-0773ca/original-reference/project/native-character/outline-converted.glb'
document,_=read_glb(source)
parents={c:i for i,n in enumerate(document['nodes']) for c in n.get('children',[])};worlds={}
def world(index):
    if index not in worlds:
        n=document['nodes'][index]
        local=np.array(n['matrix']).reshape(4,4,order='F') if 'matrix' in n else trs_matrix([n.get('translation',[0,0,0]),n.get('rotation',[0,0,0,1]),n.get('scale',[1,1,1])])
        worlds[index]=(world(parents[index]) if index in parents else np.eye(4))@local
    return worlds[index]
result=[]
for i,node in enumerate(document['nodes']):
    name=node.get('name','')
    if 'capture_material_frame' in name or name in ['Bip001_Head','Bip001_Pelvis']:
        result.append({'node':name,'index':i,'local':node,'world':world(i).tolist()})
(run/'original-material-frame.json').write_text(json.dumps(result,indent=2)+'\n')
print(json.dumps(result,indent=2))
