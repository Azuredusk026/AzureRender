from pathlib import Path
import subprocess,sys,time
root=Path.cwd();work=root/'build/p4'
for config,log,detail in [('Debug','debug-final-full.log','debug-final-detail.log'),('Release','release-full.log','release-detail.log')]:
 print('START',config,flush=True)
 with (work/log).open('w',encoding='utf-8') as output:
  result=subprocess.run(['ctest','--test-dir',str(root/('build/ninja-msvc-'+config.lower())),'-C',config,'--output-on-failure','-j','1'],stdout=output,stderr=subprocess.STDOUT)
 subprocess.run([sys.executable,'tools/build_provenance.py','verify','--build-dir','build/ninja-msvc-'+config.lower(),'--config',config],check=True)
 (work/detail).write_bytes((root/('build/ninja-msvc-'+config.lower())/'Testing/Temporary/LastTest.log').read_bytes())
 print('END',config,result.returncode,flush=True)
 if result.returncode:raise SystemExit(result.returncode)
