from pathlib import Path
import os,subprocess,sys,json,hashlib
root=Path(__file__).resolve().parent
build=Path('D:/Projects/FYP/Project/AzureRender/build/runs/20261109-220112-thesis-qualification-21b89c/build')
source=Path('D:/Projects/FYP-worktrees/thesis-paths/Project/AzureRender/experiments/thesis')
rd=Path('D:/Tools/RenderDoc/1.46/RenderDoc_1.46_64')
env=dict(os.environ,VULKAN_SDK='D:/Tools/VulkanSDK/1.4.350.0',VCPKG_ROOT='D:/Tools/vcpkg',PYTHONUTF8='1')
msvc='D:/Projects/FYP/Project/AzureRender/tools/msvc_env.bat'
def execute(name,cmd,environment=env):
 with (root/(name+'.log')).open('w',encoding='utf-8') as log:
  r=subprocess.run(cmd,env=environment,stdout=log,stderr=subprocess.STDOUT,timeout=90)
 print(name,r.returncode,flush=True)
 if r.returncode:
  print((root/(name+'.log')).read_text(encoding='utf-8',errors='replace'))
  sys.exit(r.returncode)
execute('configure',[msvc,'cmake','-S',str(source),'-B',str(build),'-G','Ninja','-DCMAKE_BUILD_TYPE=Release','-DPython3_EXECUTABLE='+sys.executable,'-DTHESIS_RENDERDOC_DIR='+str(rd)])
execute('build',[msvc,'cmake','--build',str(build),'--parallel','2'])
execute('ctest',[msvc,'ctest','--test-dir',str(build),'--output-on-failure'])
binary=build/'AzureThesisBenchmark.exe'
result=dict(binarySha256=hashlib.sha256(binary.read_bytes()).hexdigest(),cases=[],captures=[],scope='correctness and timing pilot; no formal qualification')
for gpu in ['NVIDIA','Intel']:
 for width,height in [(1920,1080),(2560,1440)]:
  for complexity in ['low','high']:
   reference=None
   for path in ['traditional','subpasses','drlr']:
    name=f'{gpu}-{width}-{complexity}-{path}'
    directory=root/name
    execute(name,[str(binary),'--shaders',str(build/'shaders'),'--output',str(directory),'--gpu',gpu,'--width',str(width),'--height',str(height),'--complexity',complexity,'--path',path,'--warmup','3','--frames','12','--validation'])
    run=json.loads((directory/'run.json').read_text())
    assert run['validationErrors']==run['validationWarnings']==0,run
    assert len(run['gpuMs'])==12 and all(x>0 for x in run['gpuMs']),run
    image=(directory/'image.rgba').read_bytes()
    assert len(image)==width*height*4
    if reference is None: reference=image
    else: assert image==reference, name+' output differs'
    result['cases'].append(dict(name=name,outputSha256=hashlib.sha256(image).hexdigest(),**run))
    (root/'pilot-summary.json').write_text(json.dumps(result,indent=2)+'\n')
captureEnv=dict(env,VK_LAYER_PATH=str(rd)+';D:/Tools/VulkanSDK/1.4.350.0/Bin',VK_INSTANCE_LAYERS='VK_LAYER_RENDERDOC_Capture',ENABLE_VULKAN_RENDERDOC_CAPTURE='1')
for path in ['traditional','subpasses','drlr']:
 directory=root/('capture-'+path);directory.mkdir()
 execute('capture-'+path,[str(binary),'--shaders',str(build/'shaders'),'--output',str(directory),'--gpu','NVIDIA','--width','128','--height','128','--path',path,'--warmup','0','--frames','2','--capture',str(directory/'frame'),'--renderdoc',str(rd/'renderdoc.dll')],captureEnv)
 captures=list(directory.glob('*.rdc'));assert captures,'No independent capture file for '+path
 capture=captures[0]
 execute('xml-'+path,[str(rd/'renderdoccmd.exe'),'convert','-f',str(capture),'-o',str(directory/'capture.xml'),'-c','xml'])
 text=(directory/'capture.xml').read_text(encoding='utf-8')
 required=['vkCmdBeginRenderPass','vkCmdDraw']
 if path=='subpasses':required+=['vkCmdNextSubpass']
 if path=='drlr':required+=['vkCmdBeginRendering','vkCmdSetRenderingInputAttachmentIndices','vkCmdPipelineBarrier']
 for token in required:assert token in text, 'capture lacks '+token
 result['captures'].append(dict(path=path,captureSha256=hashlib.sha256(capture.read_bytes()).hexdigest(),verifiedCommands=required))
 (root/'pilot-summary.json').write_text(json.dumps(result,indent=2)+'\n')
print('24 desktop correctness cases and three independent API captures passed')
