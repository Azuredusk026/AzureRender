"""RenderDoc embedded Python entry; reuse the repository's explicit capture worker."""
import hashlib,json,os,sys,traceback
from pathlib import Path
import renderdoc as rd
run=Path('D:/Projects/FYP/Project/AzureRender/build/runs/20261101-120125-character-environment-reception-fa70a7');root=run.parents[2];sys.path.insert(0,str(root/'tools/native_render'));from renderdoc_worker import capture_target
out=run/'rdc-serial-frame9-layer';out.mkdir(exist_ok=False);working=out/'target';working.mkdir();player=root/'build/ninja-msvc-release/AzurePlayer.exe';build=json.loads((run/'release-build.json').read_text());assert hashlib.sha256(player.read_bytes()).hexdigest()==build['programs']['AzurePlayer']
request=dict(operation='capture',source=dict(path=str(player),bytes=player.stat().st_size,sha256=build['programs']['AzurePlayer']),output=str(out),working_directory=str(working),frame=9,timeout=900,arguments=['--disable-parallel-recording','--project',str(run/'candidate-project/full-world-hidden.azureproject'),'--width','2560','--height','1440','--capture-frames','12','--capture-fps','60','--fixed-frame-step','--qa-camera','scene','--qa-light','scene','--capture-dir',str(working/'frames'),'--runtime-report',str(working/'runtime.json')]);(out/'request.json').write_bytes((json.dumps(request,indent=2)+'\n').encode());receipt=dict(format='RenderDocEvidence/1',operation='capture',source=request['source'],status='running',artifacts=[],tool_version=rd.GetVersionString())

# Process-only layer discovery follows Khronos' LoaderLayerInterface.md.
# https://github.com/KhronosGroup/Vulkan-Loader/blob/main/docs/LoaderLayerInterface.md
original_execute=rd.ExecuteAndInject
def scoped_execute(*args):
 names=['VK_ADD_LAYER_PATH','VK_INSTANCE_LAYERS','ENABLE_VULKAN_RENDERDOC_CAPTURE'];previous={name:os.environ.get(name) for name in names}
 try:
  layer='D:/Tools/RenderDoc/1.46/RenderDoc_1.46_64'
  os.environ['VK_ADD_LAYER_PATH']=layer+(os.pathsep+previous['VK_ADD_LAYER_PATH'] if previous['VK_ADD_LAYER_PATH'] else '')
  os.environ['VK_INSTANCE_LAYERS']='VK_LAYER_RENDERDOC_Capture'+(os.pathsep+previous['VK_INSTANCE_LAYERS'] if previous['VK_INSTANCE_LAYERS'] else '')
  os.environ['ENABLE_VULKAN_RENDERDOC_CAPTURE']='1';receipt['captureEnvironment']={name:os.environ[name] for name in names}
  return original_execute(*args)
 finally:
  for name,value in previous.items():
   if value is None:os.environ.pop(name,None)
   else:os.environ[name]=value
rd.ExecuteAndInject=scoped_execute

try:capture_target(rd,request,receipt)
except Exception:receipt.update(status='failed',error=traceback.format_exc())
finally:(out/'receipt.json').write_bytes((json.dumps(receipt,indent=2)+'\n').encode())
sys.exit(0)
