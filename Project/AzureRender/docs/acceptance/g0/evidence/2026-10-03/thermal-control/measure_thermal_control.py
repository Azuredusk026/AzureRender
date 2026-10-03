from pathlib import Path
import sys,json,subprocess,time
sys.path.insert(0,str(Path('tools').resolve()))
from run_blackhole_optimization import run,validate_report,evaluate_budget
out=Path('build/g0/thermal-controlled');out.mkdir(exist_ok=True)
results={label:{'status':'active','mode':'performance','thermalStartMaximumC':55,'runs':[]} for label in ['candidate','baseline']}
with (out/'gpu-state.csv').open('w') as log:
 monitor=subprocess.Popen(['nvidia-smi','--query-gpu=timestamp,temperature.gpu,pstate,clocks.current.graphics,power.draw,utilization.gpu','--format=csv','-l','1'],stdout=log,stderr=subprocess.STDOUT)
 try:
  for repeat in range(3):
   for label in (['candidate','baseline'] if repeat%2==0 else ['baseline','candidate']):
    start=time.monotonic()
    while True:
     temp=int(subprocess.check_output(['nvidia-smi','--query-gpu=temperature.gpu','--format=csv,noheader,nounits'],text=True).strip())
     if temp<=55:break
     if time.monotonic()-start>120:raise RuntimeError('GPU did not cool to matched starting temperature')
     time.sleep(5)
    exe=Path('build/ninja-msvc-release/AzureRender.exe' if label=='candidate' else 'build/g0/baseline/bin/AzureRender.exe').resolve()
    directory=out/label;directory.mkdir(exist_ok=True);name=f'timing-{repeat}';path=directory/(name+'.json')
    seconds=run(exe,['--scene-type','blackhole','--blackhole-quality','cinematic','--blackhole-camera','front','--fixed-frame-step','--width','1280','--height','720','--smoke-frames','300','--gpu-timing','--gpu-timing-output',str(path.resolve())],directory,name)
    report=json.loads(path.read_text());validate_report(report)
    results[label]['runs'].append({'report':report,'processSecondsIncludingStartup':seconds,'startTemperatureC':temp,'cooldownSeconds':time.monotonic()-start-seconds})
    print(f"{label} round {repeat}, start {temp}C: {report['totalAverageMs']} ms",flush=True)
  for label,result in results.items():
   result['budget']=evaluate_budget([entry['report']['totalAverageMs'] for entry in result['runs']]);result['status']='passed' if result['budget']['passed'] else 'failed'
   (out/label/'summary.json').write_text(json.dumps(result,indent=2)+'\n')
  (out/'comparison.json').write_text(json.dumps({label:result['budget'] for label,result in results.items()},indent=2)+'\n')
 finally:monitor.terminate();monitor.wait()
 if results['candidate']['status']!='passed':raise RuntimeError('Controlled GPU budget failed')
