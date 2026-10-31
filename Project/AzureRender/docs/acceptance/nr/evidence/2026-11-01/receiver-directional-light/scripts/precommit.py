import hashlib,json,re
from pathlib import Path
from urllib.parse import unquote,urlsplit
root=Path.cwd();run=Path(__file__).resolve().parent;sha=lambda p:hashlib.sha256(p.read_bytes()).hexdigest()
protected=json.loads((root/'build/runs/20261031-053326-startup-context-d7dce6/preserved-user-files.json').read_text())
for name,digest in protected.items():assert sha(root/name)==digest,name
source=root/'build/runs/20261031-183314-full-scene-playable-validation-d5257e/project'
selected=[source/'content-character/hero-restored-inputs-v8.glb']
selected+=list((source/'assets/restored-character-materials').glob('*'))
selected+=list((source/'native-gi').glob('*'));inputs=[]
for p in selected:
    if not p.is_file():continue
    q=run/'project'/p.relative_to(source);assert sha(p)==sha(q),p
    inputs.append({'file':p.relative_to(source).as_posix(),'sha256':sha(p)})
(run/'source-inputs.json').write_text(json.dumps({'status':'matched-source','files':inputs},indent=2)+'\n')
docs=['README.md','CHANGELOG.md','docs/acceptance/nr/receiver-directional-light.md',
    'docs/plans/azure-engine-plan.md','docs/plans/2026-10-31-projected-footprint-quality.md',
    'docs/runtime/directional-lights.md','docs/runtime/light-receivers.md','docs/runtime/npr-illumination.md']
links=0
for name in docs:
    p=root/name;text=re.sub(r'```.*?```','',p.read_text(),flags=re.S)
    for match in re.finditer(r'\[[^\]]*\]\(([^\s)]+)\)',text):
        target=urlsplit(match.group(1))
        if not target.scheme and target.path:assert (p.parent/unquote(target.path)).exists(),(name,target.path)
        links+=1
    for paragraph in text.split('\n\n'):
        if not paragraph.strip() or paragraph.lstrip().startswith(('#','|','- ','1.','>')):continue
        for sentence in re.split('[。！？]',paragraph):
            assert len(re.findall('[\u4e00-\u9fff]',sentence))<=45,(name,sentence[:120])
        assert len(re.findall('[。！？]',paragraph))<=4,(name,paragraph[:120])
(run/'precommit.json').write_text(json.dumps({'status':'passed','protectedUserFiles':len(protected),'frozenInputFiles':len(inputs),'linksChecked':links,'paragraphAndSentenceAudit':'passed'},indent=2)+'\n')
print('Protected user files',len(protected),'frozen input files',len(inputs),'links',links,'document audit passed')
