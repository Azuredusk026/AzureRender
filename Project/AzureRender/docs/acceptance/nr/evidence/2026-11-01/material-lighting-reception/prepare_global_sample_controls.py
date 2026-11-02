import hashlib,json,shutil,subprocess
from pathlib import Path
run=Path(__file__).resolve().parent;root=run.parents[2];out=run/'global-sample-controls';out.mkdir(exist_ok=False);rows=[]
original=json.loads((run/'depth-sample-experiment-build.json').read_text())['variants'][0];rows.append(original)
for mode in ['positive-color','fine-all','coarse-all']:
 folder=out/mode;shutil.copytree(root/'shaders',folder)
 if mode=='positive-color':
  p=folder/'mesh.frag';text=p.read_text();needle='void main() {';assert text.count(needle)==1;text=text.replace(needle,needle+'\n    outputColor=vec4(100,0,0,1);\n    outputNormal=vec4(.5,.5,1,0);\n    outputDisplayMask=vec4(1,0,0,1);\n    return;\n');p.write_bytes(text.encode())
 else:
  suffix='Fine' if mode=='fine-all' else 'Coarse';p=folder/'unlit_surface_sample.glsl';text=p.read_text();assert text.count('texture(AZ_TEX_')==4;text=text.replace('texture(AZ_TEX_','azureDiagnosticUnlitSample(AZ_TEX_');helper='vec4 azureDiagnosticUnlitSample(sampler2D image,vec2 uv){return textureGrad(image,uv,dFdx'+suffix+'(uv),dFdy'+suffix+'(uv));}\n';text=text.replace('vec4 azureSampleUnlitSurface(',helper+'vec4 azureSampleUnlitSurface(');p.write_bytes(text.encode())
 target=out/(mode+'.spv');command=['D:/Tools/VulkanSDK/1.4.350.0/Bin/glslc.exe','-DAZURE_PROBE_FIELD=1','-DAZURE_IRRADIANCE_CLIPMAP=1','-DAZURE_OBJECT_SHADOW=1','-DAZURE_BINDLESS=1',str(folder/'mesh.frag'),'-o',str(target)];result=subprocess.run(command,capture_output=True);assert result.returncode==0,result.stderr.decode(errors='replace');rows.append(dict(mode=mode,shader=target.relative_to(run).as_posix(),sha256=hashlib.sha256(target.read_bytes()).hexdigest(),command=command))
rows.append(json.loads((run/'depth-sample-experiment-build.json').read_text())['variants'][1]);rows.append(json.loads((run/'clock-experiment-build.json').read_text())['variants'][-1]);(run/'global-sample-controls-build.json').write_bytes((json.dumps(dict(status='compiled-isolated-global-controls',variants=rows,scope='Capture-only shader replacement, including a visible positive control; production files remain frozen.'),indent=2)+'\n').encode());print(json.dumps(dict(variants=[v['mode'] for v in rows])))
