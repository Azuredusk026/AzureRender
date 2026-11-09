from pathlib import Path
import json,subprocess,sys
root=Path(__file__).resolve().parent
source=Path('D:/Projects/FYP-worktrees/thesis-paths/Project/AzureRender/experiments/thesis')
build=Path('D:/Projects/FYP/Project/AzureRender/build/runs/20261109-220112-thesis-qualification-21b89c/build')
adb='D:/Tools/RenderDoc/1.46/RenderDoc_1.46_64/plugins/android/adb.exe'
remote='/data/local/tmp/azurerender-thesis-paths'
def run(name,cmd,expected=0):
 with (root/(name+'.log')).open('w',encoding='utf-8') as log:
  r=subprocess.run(cmd,stdout=log,stderr=subprocess.STDOUT,timeout=90)
 print(name,r.returncode,flush=True)
 if (r.returncode!=0)!=(expected!=0):
  print((root/(name+'.log')).read_text(encoding='utf-8',errors='replace'));sys.exit(1)
compiler=str(root/'toolchain/bin/clang.exe')
files=[str(source/'src'/p) for p in ['benchmark.cpp','Subpasses.cpp','Dynamic.cpp','Capture.cpp','Window.cpp']]
run('benchmark-compile',[compiler,'--driver-mode=g++','--target=aarch64-linux-android35','-std=c++17','-O2','-static-libstdc++','-ID:/Tools/VulkanSDK/1.4.350.0/Include',*files,'-lvulkan','-o',str(root/'benchmark')])
run('benchmark-mkdir',[adb,'-s','df0a81dc','shell','mkdir','-p',remote])
run('benchmark-push',[adb,'-s','df0a81dc','push',str(root/'benchmark'),remote+'/benchmark'])
run('benchmark-shaders',[adb,'-s','df0a81dc','push',str(build/'shaders'),remote+'/shaders'])
run('benchmark-chmod',[adb,'-s','df0a81dc','shell','chmod','700',remote+'/benchmark'])
for path in ['traditional','subpasses','drlr']:
 command=[adb,'-s','df0a81dc','shell',remote+'/benchmark','--path',path,'--gpu','Adreno','--shaders',remote+'/shaders','--output',remote+'/'+path,'--width','128','--height','128','--warmup','2','--frames','8']
 run('mobile-'+path,command,expected=1 if path=='drlr' else 0)
 if path!='drlr':
  run('pull-'+path,[adb,'-s','df0a81dc','pull',remote+'/'+path,str(root/('benchmark-'+path))])
  data=json.loads((root/('benchmark-'+path)/'run.json').read_text())
  assert not data['validationEnabled'] and len(data['gpuMs'])==8 and all(t>0 for t in data['gpuMs'])
  assert data['nextSubpassCalls']==(10 if path=='subpasses' else 0)
 else: assert 'DRLR extension unavailable' in (root/('mobile-'+path+'.log')).read_text()
assert (root/'benchmark-traditional/image.rgba').read_bytes()==(root/'benchmark-subpasses/image.rgba').read_bytes()
print('Mobile traditional/subpasses execute with identical images; unsupported DRLR explicitly rejected; mobile validation layer not enabled')
