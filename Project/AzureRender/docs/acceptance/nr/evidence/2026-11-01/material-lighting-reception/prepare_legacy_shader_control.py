import hashlib,json,re,shutil,subprocess
from pathlib import Path
run=Path(__file__).resolve().parent;root=run.parents[2];out=run/'legacy-shader-control';out.mkdir(exist_ok=True);src=out/'source';shutil.copytree(root/'shaders',src,dirs_exist_ok=True);old=subprocess.check_output(['git','show','e2c5bbb:Project/AzureRender/shaders/mesh.frag']);(src/'mesh.frag').write_bytes(old);build=json.loads((run/'release-build.json').read_text());reference=json.loads((root/'build/runs/20261101-043757-material-display-transform-93d6e0/release-final-build.json').read_text());sha=lambda p:hashlib.sha256(p.read_bytes()).hexdigest();changed={n for n,v in build['shaders'].items() if n in reference['shaders'] and v!=reference['shaders'][n]};commands={}
for line in (root/'build/ninja-msvc-release/build.ninja').read_text().splitlines():
 if 'glslc.exe ' not in line or ' -MD -MF ' not in line:continue
 match=re.search(r'glslc\.exe(.*?)\s+-MD -MF \S+\s+\S+/shaders/(\S+)\s+-o\s+\S+/shaders/(\S+\.spv)',line)
 if match:commands[match[3]]=(match[1].split(),match[2])
assert changed and changed<=set(commands),(len(changed),changed-set(commands));roots={}
for mode in ['main-only','all-legacy']:
 folder=out/mode/'shaders';folder.mkdir(parents=True,exist_ok=True)
 for p in (root/'build/ninja-msvc-release/shaders').glob('*.spv'):shutil.copy2(p,folder/p.name)
 roots[mode]=folder.parent
rows=[]
for name in sorted(changed):
 flags,source=commands[name];target=roots['all-legacy']/'shaders'/name;command=['D:/Tools/VulkanSDK/1.4.350.0/Bin/glslc.exe',*flags,str(src/source),'-o',str(target)];result=subprocess.run(command,capture_output=True);assert result.returncode==0,result.stderr.decode(errors='replace');digest=sha(target);assert digest==reference['shaders'][name],(name,digest,reference['shaders'][name]);rows.append(dict(name=name,source=source,sha256=digest,currentSha256=build['shaders'][name],command=command))
name='mesh_object_bindless_clipmap_field.frag.spv';shutil.copy2(roots['all-legacy']/'shaders'/name,roots['main-only']/'shaders'/name)
for name,digest in build['shaders'].items():assert sha(root/'build/ninja-msvc-release/shaders'/name)==digest
report=dict(status='compiled-byte-exact-legacy-shader-controls',meshSourceCommit='e2c5bbb',meshSourceSha256=hashlib.sha256(old).hexdigest(),referenceRun='build/runs/20261101-043757-material-display-transform-93d6e0',shaderRoots={k:str(v) for k,v in roots.items()},replacedShaders=rows,scope='Hidden-character default-environment diagnosis only; installed shaders and production files remain frozen.');(run/'legacy-shader-control-build.json').write_bytes((json.dumps(report,indent=2)+'\n').encode());print(json.dumps(dict(status=report['status'],byteExactReferenceShaders=len(rows),roots=report['shaderRoots'])))
