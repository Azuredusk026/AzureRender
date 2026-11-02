from pathlib import Path
run=Path(__file__).resolve().parent;text=(run/'capture_parallel_rdc.py').read_text().replace('import hashlib,json,sys,traceback','import hashlib,json,os,sys,traceback').replace("out=run/'rdc-parallel-frame4'","out=run/'rdc-layer-smoke'")
text=text.replace("str(run/'candidate-project/full-world-hidden.azureproject')","json.loads((run/'public-gpu/release/summary.json').read_text())['cases'][0]['command'][2]").replace("'2560','--height','1440','--capture-frames','12'","'128','--height','128','--capture-frames','30'")
scope="""
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
"""
text=text.replace('try:capture_target(rd,request,receipt)',scope+'\ntry:capture_target(rd,request,receipt)');assert "'--width','128'" in text and "'--capture-frames','30'" in text;(run/'capture_layer_smoke.py').write_bytes(text.encode());print('Prepared process-scoped RenderDoc capture layer smoke')
