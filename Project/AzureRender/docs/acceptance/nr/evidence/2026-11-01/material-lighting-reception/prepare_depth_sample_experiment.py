import hashlib,json,shutil,subprocess
from pathlib import Path
run=Path(__file__).resolve().parent;root=run.parents[2];out=run/'unlit-depth-sample-experiment';out.mkdir(exist_ok=False);build=json.loads((run/'release-build.json').read_text());rows=[];sha=lambda p:hashlib.sha256(p.read_bytes()).hexdigest()
for mode in ['original','point-depth']:
 folder=out/mode;shutil.copytree(root/'shaders',folder)
 if mode=='point-depth':
  p=folder/'unlit_surface_sample.glsl';text=p.read_text();old='texture(overlaySceneDepth,AZURE_FRAGMENT_COORD.xy/camera.clusterDepth.zw).r';assert text.count(old)==1;text=text.replace(old,'texelFetch(overlaySceneDepth,ivec2(AZURE_FRAGMENT_COORD.xy),0).r');p.write_bytes(text.encode())
 target=out/(mode+'.spv');command=['D:/Tools/VulkanSDK/1.4.350.0/Bin/glslc.exe','-DAZURE_PROBE_FIELD=1','-DAZURE_IRRADIANCE_CLIPMAP=1','-DAZURE_OBJECT_SHADOW=1','-DAZURE_BINDLESS=1',str(folder/'mesh.frag'),'-o',str(target)];result=subprocess.run(command,capture_output=True);assert result.returncode==0,result.stderr.decode(errors='replace');rows.append(dict(mode=mode,shader=target.relative_to(run).as_posix(),sha256=sha(target),command=command))
assert rows[0]['sha256']==build['shaders']['mesh_object_bindless_clipmap_field.frag.spv']
for name,digest in build['shaders'].items():assert sha(root/'build/ninja-msvc-release/shaders'/name)==digest
(run/'depth-sample-experiment-build.json').write_bytes((json.dumps(dict(status='compiled-isolated-depth-sampling-controls',variants=rows,scope='Capture-only same-pixel depth diagnosis; production source and installed shaders remain frozen.'),indent=2)+'\n').encode())
text=(run/'replay_derivative_experiment.py').read_text().replace('derivative-experiment-build.json','depth-sample-experiment-build.json').replace("run/'derivative-replay'","run/'depth-sample-replay'").replace("run/'derivative-replay.json'","run/'depth-sample-replay.json'").replace('measured-derivative-controls','measured-depth-sampling-controls');(run/'replay_depth_sample_experiment.py').write_bytes(text.encode());print(json.dumps([dict(mode=v['mode'],sha256=v['sha256']) for v in rows]))
