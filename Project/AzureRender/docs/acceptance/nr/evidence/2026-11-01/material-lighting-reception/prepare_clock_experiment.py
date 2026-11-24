import hashlib,json,shutil,subprocess
from pathlib import Path
run=Path(__file__).resolve().parent;root=run.parents[2];out=run/'unlit-clock-experiment';out.mkdir(exist_ok=False);build=json.loads((run/'release-build.json').read_text());rows=[];sha=lambda p:hashlib.sha256(p.read_bytes()).hexdigest()
for mode in ['original','tau-minus','tau-plus','precise-uv']:
 folder=out/mode;shutil.copytree(root/'shaders',folder)
 if mode.startswith('tau-'):
  p=folder/'unlit_surface_sample.glsl';text=p.read_text();old='float tau=p.rows[5].x*camera.unlitTimeExposure.x+p.rows[5].y;';assert text.count(old)==1
  operator='-' if mode=='tau-minus' else '+';text=text.replace(old,old+'\n    if(material.materialPadding==1406U)tau=uintBitsToFloat(floatBitsToUint(tau)'+operator+'1U);');p.write_bytes(text.encode())
 elif mode=='precise-uv':
  p=folder/'unlit_surface_sample.glsl';text=p.read_text().replace('float tau=p.rows[5].x','precise float tau=p.rows[5].x');p.write_bytes(text.encode())
  p=folder/'unlit_texture_layers.glsl';text=p.read_text().replace('vec2 d=uv+','precise vec2 d=uv+').replace('vec2 rotated=','precise vec2 rotated=');old='return rotated*scaleOffset.xy+scaleOffset.zw;';assert text.count(old)==1;text=text.replace(old,'precise vec2 result=rotated*scaleOffset.xy+scaleOffset.zw;\n    return result;');p.write_bytes(text.encode())
 target=out/(mode+'.spv');command=['D:/Tools/VulkanSDK/1.4.350.0/Bin/glslc.exe','-DAZURE_PROBE_FIELD=1','-DAZURE_IRRADIANCE_CLIPMAP=1','-DAZURE_OBJECT_SHADOW=1','-DAZURE_BINDLESS=1',str(folder/'mesh.frag'),'-o',str(target)];result=subprocess.run(command,capture_output=True);assert result.returncode==0,result.stderr.decode(errors='replace')
 rows.append(dict(mode=mode,shader=target.relative_to(run).as_posix(),sha256=sha(target),command=command))
assert rows[0]['sha256']==build['shaders']['mesh_object_bindless_clipmap_field.frag.spv']
for name,digest in build['shaders'].items():assert sha(root/'build/ninja-msvc-release/shaders'/name)==digest
(run/'clock-experiment-build.json').write_bytes((json.dumps(dict(status='compiled-isolated-clock-controls',variants=rows,scope='Capture-only numerical diagnosis; production source and installed shaders remain frozen.'),indent=2)+'\n').encode());print(json.dumps([dict(mode=v['mode'],sha256=v['sha256']) for v in rows]))
text=(run/'replay_derivative_experiment.py').read_text().replace('derivative-experiment-build.json','clock-experiment-build.json').replace("run/'derivative-replay'","run/'clock-replay'").replace("run/'derivative-replay.json'","run/'clock-replay.json'").replace('measured-derivative-controls','measured-clock-controls');(run/'replay_clock_experiment.py').write_bytes(text.encode())
