from pathlib import Path
import subprocess,json
out=Path('build/g0/gpu-variation');out.mkdir(exist_ok=True)
with (out/'gpu-state.csv').open('w') as log:
 monitor=subprocess.Popen(['nvidia-smi','--query-gpu=timestamp,temperature.gpu,pstate,clocks.current.graphics,clocks.current.memory,power.draw,utilization.gpu','--format=csv','-l','1'],stdout=log,stderr=subprocess.STDOUT)
 try:
  results={}
  for label,exe in [('baseline','build/g0/baseline/bin/AzureRender.exe'),('candidate','build/ninja-msvc-release/AzureRender.exe')]:
   p=subprocess.run(['python','tools/run_blackhole_optimization.py','--executable',exe,'--output-dir',str(out/label),'--mode','performance'],stdout=subprocess.PIPE,stderr=subprocess.STDOUT,text=True)
   (out/(label+'.log')).write_text(p.stdout,encoding='utf-8')
   results[label]=json.loads((out/label/'summary.json').read_text())['budget']
  (out/'comparison.json').write_text(json.dumps(results,indent=2)+'\n')
  print(json.dumps(results))
 finally:
  monitor.terminate();monitor.wait()
