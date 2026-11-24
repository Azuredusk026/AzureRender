from pathlib import Path
from zipfile import ZipFile
import hashlib,json,os,subprocess,sys
root=Path(__file__).resolve().parent
archive=Path('D:/Tools/UnityAndroid-6000.6.7f1/android-ndk-r27c.zip')
prefix='android-ndk-r27c/toolchains/llvm/prebuilt/windows-x86_64/'
def keep(name):
 if not name.startswith(prefix): return False
 t=name[len(prefix):]
 return t in ['bin/clang.exe','bin/ld.lld.exe','bin/libxml2.dll','bin/libwinpthread-1.dll'] or t.startswith(('sysroot/usr/include/','sysroot/usr/lib/aarch64-linux-android/')) or ('lib/clang/' in t and ('/include/' in t or 'aarch64' in t))
with ZipFile(archive) as z:
 entries=[i for i in z.infolist() if not i.is_dir() and keep(i.filename)]
 size=sum(i.file_size for i in entries)
 if size>450*2**20 or len(entries)>5000: raise RuntimeError('minimal NDK estimate exceeded')
 for entry in entries:
  target=root/'toolchain'/entry.filename[len(prefix):]
  if not target.resolve().is_relative_to((root/'toolchain').resolve()): raise RuntimeError('archive path escape')
  target.parent.mkdir(parents=True,exist_ok=True)
  if not target.exists(): target.write_bytes(z.read(entry))
 (root/'toolchain-manifest.json').write_text(json.dumps(dict(archive=str(archive),bytes=size,files=len(entries),archiveSha256=hashlib.sha256(archive.read_bytes()).hexdigest()),indent=2)+'\n')
print('Minimal NDK extracted',size,flush=True)
adb='D:/Tools/RenderDoc/1.46/RenderDoc_1.46_64/plugins/android/adb.exe'
compiler=str(root/'toolchain/bin/clang.exe')
source=Path('D:/Projects/FYP-worktrees/thesis-paths/Project/AzureRender/experiments/thesis/src/qualification.cpp')
env=dict(os.environ,PYTHONUTF8='1')
remote='/data/local/tmp/azurerender-thesis-qualification'
def run(name,cmd):
 with (root/(name+'.log')).open('w',encoding='utf-8') as log:
  r=subprocess.run(cmd,env=env,stdout=log,stderr=subprocess.STDOUT,timeout=90)
 print(name,r.returncode,flush=True)
 if r.returncode:
  print((root/(name+'.log')).read_text(encoding='utf-8',errors='replace'))
  sys.exit(r.returncode)
run('compile',[compiler,'--driver-mode=g++','--target=aarch64-linux-android35','-std=c++17','-O2','-static-libstdc++','-ID:/Tools/VulkanSDK/1.4.350.0/Include',str(source),'-lvulkan','-o',str(root/'qualification')])
run('mkdir',[adb,'-s','df0a81dc','shell','mkdir','-p',remote])
run('push',[adb,'-s','df0a81dc','push',str(root/'qualification'),remote+'/qualification'])
run('chmod',[adb,'-s','df0a81dc','shell','chmod','700',remote+'/qualification'])
run('query',[adb,'-s','df0a81dc','shell',remote+'/qualification',remote+'/capabilities.json','--attempt-local-read'])
run('pull',[adb,'-s','df0a81dc','pull',remote+'/capabilities.json',str(root/'mobile-native-capabilities.json')])
run('evaluate',[sys.executable,'-B','D:/Projects/FYP-worktrees/thesis-paths/Project/AzureRender/experiments/thesis/tools/qualification.py',str(root/'mobile-native-capabilities.json'),'--output',str(root/'mobile-native-qualification.json')])
