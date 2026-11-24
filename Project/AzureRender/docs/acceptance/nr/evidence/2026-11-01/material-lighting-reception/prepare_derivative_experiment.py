import hashlib,json,shutil,subprocess
from pathlib import Path
run=Path(__file__).resolve().parent;root=run.parents[2];out=run/'unlit-derivative-experiment';out.mkdir(exist_ok=True);build=json.loads((run/'release-build.json').read_text());rows=[];sha=lambda p:hashlib.sha256(p.read_bytes()).hexdigest()
for mode in ['original','fine','coarse']:
 folder=out/mode;shutil.copytree(root/'shaders',folder,dirs_exist_ok=True)
 p=folder/'unlit_surface_sample.glsl'
 if mode!='original':
  text=p.read_text();assert text.count('texture(AZ_TEX_')==4
  text=text.replace('texture(AZ_TEX_','azureDiagnosticUnlitSample(AZ_TEX_')
  suffix='Fine' if mode=='fine' else 'Coarse'
  helper='vec4 azureDiagnosticUnlitSample(sampler2D image,vec2 uv){\n vec2 dx=dFdx'+suffix+'(uv),dy=dFdy'+suffix+'(uv);\n if(material.materialPadding==1406U)return textureGrad(image,uv,dx,dy);\n return texture(image,uv);\n}\n'
  text=text.replace('vec4 azureSampleUnlitSurface(',helper+'vec4 azureSampleUnlitSurface(');p.write_bytes(text.encode())
 target=out/(mode+'.spv');command=['D:/Tools/VulkanSDK/1.4.350.0/Bin/glslc.exe','-DAZURE_PROBE_FIELD=1','-DAZURE_IRRADIANCE_CLIPMAP=1','-DAZURE_OBJECT_SHADOW=1','-DAZURE_BINDLESS=1',str(folder/'mesh.frag'),'-o',str(target)];result=subprocess.run(command,capture_output=True);assert result.returncode==0,result.stderr.decode(errors='replace')
 rows.append(dict(mode=mode,shader=target.relative_to(run).as_posix(),sha256=sha(target),sourceSha256=sha(p),command=command))
assert rows[0]['sha256']==build['shaders']['mesh_object_bindless_clipmap_field.frag.spv'],'Control compilation must reproduce the exact captured shader'
for name,digest in build['shaders'].items():assert sha(root/'build/ninja-msvc-release/shaders'/name)==digest
(run/'derivative-experiment-build.json').write_bytes((json.dumps(dict(status='compiled-isolated-derivative-controls',variants=rows,scope='Capture-only material 703 diagnostic; production source and installed shaders remain frozen.'),indent=2)+'\n').encode());print(json.dumps([dict(mode=v['mode'],sha256=v['sha256']) for v in rows]))
