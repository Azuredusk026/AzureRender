from pathlib import Path
import hashlib,json,re,subprocess
from urllib.parse import unquote,urlsplit
root=Path.cwd();run=Path(__file__).resolve().parent
sha=lambda p:hashlib.sha256(p.read_bytes()).hexdigest()
source=root/'build/runs/20261031-172112-hair-source-inputs-14d199/project'
selected=[source/'content-character/hero-restored-inputs-v8.glb']
selected+=list((source/'assets/restored-character-materials').glob('*'))
selected+=list((source/'native-gi').glob('*'))
records=[]
for p in selected:
    if not p.is_file():continue
    q=run/'project'/p.relative_to(source);assert sha(p)==sha(q),p
    records.append({'file':p.relative_to(source).as_posix(),'sha256':sha(p)})
assert len([p for p in records if p['file'].endswith('.azurematerial')])==13
assert sha(run/'project/content-character/hero-restored-inputs-v8.glb')=='f188722c428358787a12be615d944f4a07185e9a8eaab72b9aa21105f3b7f997'
(run/'source-inputs.json').write_text(json.dumps({'status':'matched-frozen-inputs','files':records},indent=2)+'\n')
protected=json.loads((root/'build/runs/20261031-053326-startup-context-d7dce6/preserved-user-files.json').read_text())
for name,digest in protected.items():assert sha(root/name)==digest,(name,'user data changed')
docs=['README.md','CHANGELOG.md','docs/acceptance/nr/joint-visual-input-isolation.md',
    'docs/acceptance/recovery/full-scene-delivery.md','docs/acceptance/recovery/full-scene-playable.md',
    'docs/plans/2026-10-31-full-scene-playable.md','docs/plans/2026-10-31-projected-footprint-quality.md',
    'docs/plans/azure-engine-plan.md','docs/runtime/npr-illumination.md','docs/tutorials/restored-native-scene.md']
links=0
for name in docs:
    p=root/name;text=re.sub(r'```.*?```','',p.read_text(),flags=re.S)
    for match in re.finditer(r'\[[^\]]*\]\(([^\s)]+)\)',text):
        target=urlsplit(match.group(1))
        if not target.scheme and target.path:assert (p.parent/unquote(target.path)).exists(),(name,target.path)
        links+=1
    for para in text.split('\n\n'):
        if not para.strip() or para.lstrip().startswith(('#','|','- ','1.','>')):continue
        for sentence in re.split('[。！？]',para):
            assert len(re.findall('[\u4e00-\u9fff]',sentence))<=45,(name,sentence[:100])
        assert len(re.findall('[。！？]',para))<=4,(name,para[:100])
print('Frozen files',len(records),'protected user files',len(protected),'links',links,'document audit passed')
(run/'precommit.json').write_text(json.dumps({'status':'passed','frozenFiles':len(records),'protectedUserFiles':len(protected),'checkedLinks':links,'documentParagraphAndSentenceAudit':'passed'},indent=2)+'\n')
