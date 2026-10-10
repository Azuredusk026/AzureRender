"""Build a reviewed phase index while preserving pre-existing working tree edits."""
from pathlib import Path
import difflib,hashlib,json,os,shutil,subprocess,sys,io
ROOT=Path.cwd().resolve().parents[1]
PROJECT=ROOT/'Project/AzureRender'
WORK=PROJECT/'build/implementation-2026-10-10'
BASE=WORK/'baseline-p5'
initial=set(json.loads((BASE/'status.json').read_text(encoding='utf-8'))['files'])
def git(*args,env=None,**kwargs):
    result=subprocess.run(['git',*args],cwd=ROOT,env=env,capture_output=True,**kwargs)
    if result.returncode:raise RuntimeError((result.stdout+result.stderr).decode(errors='replace'))
    return result
gitdir=Path(git('rev-parse','--absolute-git-dir').stdout.decode().strip())
partial=git('diff','--cached','--name-only','-z').stdout
partial_names=[p.decode() for p in partial.split(b'\0') if p]
assert len(partial_names)==64, 'Concurrent index changes require review'
assert all(n.startswith('Project/AzureRender/docs/acceptance/p5/') or n in (
    'Project/AzureRender/README.md','Project/AzureRender/CHANGELOG.md','Project/AzureRender/CMakeLists.txt') for n in partial_names)
index_bytes=(gitdir/'index').read_bytes()
backup=WORK/'p5-interrupted-index'
if not backup.exists():backup.write_bytes(index_bytes)
(WORK/'p5-interrupted-staged.patch').write_bytes(git('diff','--cached','--binary').stdout)
temporary=WORK/'p5-complete.index'
if temporary.exists():temporary.unlink()
env=dict(os.environ,GIT_INDEX_FILE=str(temporary))
git('read-tree','HEAD',env=env)
status=git('status','--porcelain=v1','-z','--untracked-files=all').stdout
names={item[3:].decode() for item in status.split(b'\0') if item}
evidence=PROJECT/'docs/acceptance/p5/evidence'
manifest=json.loads((evidence/'manifest.json').read_text(encoding='utf-8'))
names.update((evidence/row['file']).relative_to(ROOT).as_posix() for row in manifest['files'])
names.add((evidence/'manifest.json').relative_to(ROOT).as_posix())
changed=[];new=[];preserved=[]
allowed_initial={f'Project/AzureRender/{name}' for name in (
    'README.md','CHANGELOG.md','CMakeLists.txt','docs/index.md','docs/plans/azure-engine-plan.md','mkdocs.yml')}
for name in sorted(names):
    path=ROOT/name
    if name in initial:
        saved=BASE/'files'/name
        if path.read_bytes()==saved.read_bytes():preserved.append(name);continue
        assert name in allowed_initial,('Unexpected original-user-file change',name)
        before=saved.read_text(encoding='utf-8');after=path.read_text(encoding='utf-8')
        patch=''.join(difflib.unified_diff(before.splitlines(True),after.splitlines(True),fromfile='a/'+name,tofile='b/'+name,n=1))
        if not patch:continue
        git('apply','--cached','--whitespace=nowarn',env=env,input=patch.encode())
    else:new.append(name)
    changed.append(name)
assert all(n.startswith('Project/AzureRender/') for n in changed)
assert not any('/docs/acceptance/p4/' in n or '/commands/EditService.' in n or n.endswith('test_editor_native_input.py') for n in changed)
if new:
    paths=WORK/'p5-new-paths.bin';paths.write_bytes(b''.join(n.encode()+b'\0' for n in new))
    git('add','--force','--pathspec-from-file='+str(paths),'--pathspec-file-nul',env=env)
blob_names=[(evidence/row['file']).relative_to(ROOT).as_posix() for row in manifest['files']]
blobs=io.BytesIO(git('cat-file','--batch',env=env,input=''.join(':'+n+'\n' for n in blob_names).encode()).stdout)
for name,row in zip(blob_names,manifest['files']):
    header=blobs.readline().split()
    assert len(header)==3 and header[1]==b'blob',(name,header)
    data=blobs.read(int(header[2]));assert blobs.read(1)==b'\n'
    assert hashlib.sha256(data).hexdigest()==row['sha256'],('Evidence bytes changed in index',name)
git('diff','--cached','--check',env=env)
tree=git('write-tree',env=env).stdout.decode().strip()
(WORK/'p5-staged-files.json').write_text(json.dumps(changed,ensure_ascii=False,indent=2)+'\n',encoding='utf-8')
(WORK/'p5-proposed-staged.patch').write_bytes(git('diff','--cached','--binary',env=env).stdout)
review=dict(proposedTree=tree,phaseFiles=len(changed),preservedOriginalFiles=len(preserved),partialStageFiles=len(partial_names),
            originalIndexSha256=hashlib.sha256(index_bytes).hexdigest(),unrelatedPathsIncluded=False)
(WORK/'p5-stage-recovery.json').write_text(json.dumps(review,indent=2)+'\n',encoding='utf-8')
print(json.dumps(review,indent=2))
print('\n'.join(n for n in changed if '/docs/acceptance/p5/evidence/' not in n))
if '--apply' in sys.argv:
    assert (gitdir/'index').read_bytes()==index_bytes,'Index changed concurrently'
    assert git('diff','--cached','--name-only','-z').stdout==partial
    git('read-tree',tree)
    assert git('write-tree').stdout.decode().strip()==tree
    git('diff','--cached','--check')
    print('Reviewed phase index installed; working tree bytes retained')
