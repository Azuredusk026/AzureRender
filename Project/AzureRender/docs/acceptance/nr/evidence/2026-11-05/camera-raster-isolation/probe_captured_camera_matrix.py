from pathlib import Path
import os,json,shutil,subprocess,numpy as np
r=Path(__file__).resolve().parent;root=r.parents[2];b=root/'build/ninja-msvc-release';out=r/'captured-camera-matrix-program';source=out/'source';target=out/'resources/shaders';shutil.copytree(root/'shaders',source);target.mkdir(parents=True)
for f in (b/'shaders').glob('*.spv'):shutil.copyfile(f,target/f.name)
data=json.loads((r/'native-engine-vertex-camera-blocks.json').read_bytes())[0]['blocks'];vp=[v['float32'] for v in data[0]['variables'][32:36]];jitter=data[1]['variables'][19]['float32'];origin=np.array(data[0]['variables'][44]['float32'][:3])-np.array([-300,300,-300]);fmt=lambda x:format(float(np.float32(x)),'.9g')+('' if '.' in format(float(np.float32(x)),'.9g') or 'e' in format(float(np.float32(x)),'.9g') else '.0')
helper='\nmat4 capturedCameraMvp(mat4 model) {\n    mat4 sourceProjection=mat4('+','.join(fmt(v) for col in vp for v in col)+');\n    mat4 relative=model;\n    for(int c=0;c<4;++c)relative[c].x=-relative[c].x;\n    relative[3].xyz-=vec3('+','.join(fmt(v) for v in origin)+');\n    mat4 value=sourceProjection*relative;\n    for(int c=0;c<4;++c){value[c].xy+=vec2('+fmt(-2*jitter[2])+','+fmt(2*jitter[3])+')*value[c].w;value[c].z=value[c].w-value[c].z;}\n    return value;\n}\n'
f=source/'instance_data.glsl';s=f.read_text();s=s.replace('#endif',helper+'#endif');f.write_text(s)
changes=[]
for f in source.glob('*'):
 if f.suffix not in ['.glsl','.vert']:continue
 s=f.read_text();needle='instanceData.instances[gl_InstanceIndex].modelViewProjection'
 if needle in s:
  s=s.replace(needle,'capturedCameraMvp(instanceData.instances[gl_InstanceIndex].model)');f.write_text(s);changes.append(f.name)
count=0
for line in (b/'build.ninja').read_text().splitlines():
 if not line.lstrip().startswith('COMMAND =') or 'glslc.exe ' not in line or not any('/shaders/'+name+' ' in line for name in ['mesh.vert','outline.vert']):continue
 args=line.split('glslc.exe ',1)[1].split(' &&')[0].split();name=Path(args[args.index('-o')+1].strip(chr(34))).name;entry=next(v for v in args if '/shaders/' in v and v.endswith('.vert'));src=source/Path(entry).name;dest=out/name
 arg=str(dest);arg='\\\\?\\'+arg if len(arg)>=260 else arg
 subprocess.run(['D:/Tools/VulkanSDK/1.4.350.0/Bin/glslc.exe',*[v for v in args if v.startswith('-D')],'-I',str(source),str(src),'-o',arg],check=True,stdout=subprocess.DEVNULL);os.replace(dest,target/name);count+=1
(out/'source-inputs.json').write_text(json.dumps(dict(programs=count,changes=changes,nativeViewProjectionColumns=vp,nativeJitter=jitter,localCameraOrigin=origin.tolist(),sourceSceneOrigin=[-300,300,-300],scope='Private captured-camera projection diagnosis; production programs unchanged'),indent=2));print('captured camera programs',count,flush=True);os.environ['AZURERENDER_RESOURCE_ROOT']=str(out/'resources');s=(r/'capture_public_screen_shadow_head.py').read_text().replace('public-back-view-static','qualified-lit-surfaces-static').replace('public-screen-shadow-head-capture','captured-camera-matrix-capture');exec(compile(s,'captured-camera-matrix','exec'))
