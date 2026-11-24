"""Stage only the increment over the captured user working tree."""
import json
import subprocess
import re
from pathlib import Path

ROOT=Path.cwd()
OUT=ROOT/'build/p8-format-closure-2026-10-12'
baseline=json.loads((OUT/'stage-baseline.json').read_text(encoding='utf-8'))
def canonical(text):return text.replace('\r\n','\n').replace('\r','')
staged=[]
for name, initial in baseline.items():
    initial={k:canonical(v) for k,v in initial.items()}
    current=canonical((ROOT/name).read_text(encoding='utf-8-sig'))
    if current==initial['work']:
        continue
    if initial['head']==initial['work']:
        merged=current.encode('utf-8')
    else:
        folder=OUT/'stage-merge'/name.replace('/','__')
        folder.mkdir(parents=True,exist_ok=True)
        for kind,content in [('current',current),('base',initial['work']),('head',initial['head'])]:
            (folder/kind).write_text(content,encoding='utf-8',newline='\n')
        result=subprocess.run(['git','merge-file','-p',str(folder/'current'),
            str(folder/'base'),str(folder/'head')],capture_output=True)
        merged=canonical(result.stdout.decode('utf-8'))
        if result.returncode:
            conflicts=list(re.finditer(r'^<<<<<<<[^\n]*\n(.*?)^=======\n(.*?)^>>>>>>>[^\n]*\n',merged,re.M|re.S))
            if name!='docs/plans/azure-engine-plan.md' or len(conflicts)!=1 or '| P4 编辑器交互体验验收 |' not in conflicts[0].group(1):
                raise RuntimeError('Owned increment needs manual merge: '+name)
            # The P4 status belongs to pre-existing user changes. Keep HEAD in the index.
            conflict=conflicts[0]
            merged=merged[:conflict.start()]+conflict.group(2)+merged[conflict.end():]
        merged=merged.encode('utf-8')
    blob=subprocess.run(['git','hash-object','-w','--stdin'],input=merged,
        capture_output=True,check=True).stdout.decode().strip()
    subprocess.run(['git','update-index','--add','--cacheinfo','100644',blob,
        'Project/AzureRender/'+name],cwd=ROOT.parents[1],check=True)
    staged.append(dict(path=name,blob=blob,userChangesPreserved=True))
(OUT/'staged-owned.json').write_text(json.dumps(staged,indent=2),encoding='utf-8')
print(json.dumps(staged,indent=2))
