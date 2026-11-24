from pathlib import Path
import os,subprocess,sys,json
root=Path(__file__).resolve().parent
build=Path('D:/Projects/FYP/Project/AzureRender/build/runs/20261109-220112-thesis-qualification-21b89c/build')
source=Path('D:/Projects/FYP-worktrees/thesis-paths/Project/AzureRender/experiments/thesis')
env=dict(os.environ,VULKAN_SDK='D:/Tools/VulkanSDK/1.4.350.0',VCPKG_ROOT='D:/Tools/vcpkg',PYTHONUTF8='1')
msvc='D:/Projects/FYP/Project/AzureRender/tools/msvc_env.bat'
def execute(name,cmd):
 with (root/(name+'.log')).open('w',encoding='utf-8') as log:
  r=subprocess.run(cmd,env=env,stdout=log,stderr=subprocess.STDOUT,timeout=90)
 print(name,r.returncode,flush=True)
 if r.returncode:
  print((root/(name+'.log')).read_text(encoding='utf-8',errors='replace'));sys.exit(r.returncode)
execute('window-configure',[msvc,'cmake','-S',str(source),'-B',str(build),'-DTHESIS_GLFW_DIR=D:/Projects/FYP/Project/AzureRender/build/ninja-msvc-debug/vcpkg_installed/x64-windows/share/glfw3'])
execute('window-build',[msvc,'cmake','--build',str(build),'--parallel','2'])
execute('window-ctest',[msvc,'ctest','--test-dir',str(build),'--output-on-failure'])
reference=(source/'evidence/traditional/image.rgba').read_bytes()
for path in ['traditional','subpasses','drlr']:
 directory=root/('window-'+path)
 execute('window-'+path,[str(build/'AzureThesisBenchmark.exe'),'--present','--path',path,'--width','128','--height','128','--gpu','NVIDIA','--shaders',str(build/'shaders'),'--output',str(directory),'--warmup','2','--frames','6','--validation'])
 x=json.loads((directory/'run.json').read_text())
 assert x['validationErrors']==x['validationWarnings']==0,x
 assert len(x['presentationWorkMs'])==6 and all(v>0 for v in x['presentationWorkMs']),x
 assert (directory/'image.rgba').read_bytes()==reference
execute('headless-regression',[sys.executable,'-B','D:/Projects/FYP/Project/AzureRender/build/runs/20261109-220112-thesis-qualification-21b89c/regress_paths.py'])
print('Three swapchain paths, independent presentation work, byte equality and headless regression passed')
