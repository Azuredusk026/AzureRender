import hashlib,json,os,subprocess,sys,time
from pathlib import Path
root=Path.cwd();run=Path(__file__).resolve().parent;python=Path(sys.executable)
startup=subprocess.STARTUPINFO();startup.dwFlags|=subprocess.STARTF_USESHOWWINDOW;startup.wShowWindow=0
env=os.environ.copy();env['DOTNET_ROOT']='D:/Tools/dotnet9'
report={'status':'running','jobs':[]}
jobs=[('npr-release-all',[python,root/'tools/test_npr_illumination_gpu.py','--executable',root/'build/ninja-msvc-release/AzurePlayer.exe','--output',run/'gpu-final-release-all','--expected-validation','disabled']),
    ('full-scene-regression',[python,run/'capture_character.py','--case','full-control','--case','hidden-control','--case','hidden-reference-light'])]
for cfg,validation in [('release','disabled'),('debug','enabled')]:
    player=root/f'build/ninja-msvc-{cfg}/AzurePlayer.exe'
    for name,tool,cases in [
        ('hair','test_projected_hair_lighting_gpu.py',['direct','ambient','kk','kk-fixed','no-main','local-kk','extra-kk','disabled-lighting','local-specular-kk']),
        ('eye','test_eye_npr_gpu.py',['main','combined','shadow-zero','shadow-one','fixed','document']),
        ('object','test_object_shadow_gpu.py',['shadow','shadow-fixed','two-objects','receiver-combined','hair-object','hair-object-fixed','hair-object-ambient'])]:
        cmd=[python,root/'tools'/tool,'--executable',player,'--output',run/f'{name}-{cfg}','--expected-validation',validation]
        for case in cases:cmd+=['--case',case]
        jobs.append((f'{name}-{cfg}',cmd))
try:
    for name,command in jobs:
        command=[str(x) for x in command];row={'job':name,'command':command,'status':'running'};report['jobs'].append(row)
        (run/'remaining-status.json').write_text(json.dumps(report,indent=2)+'\n');print('START',name,flush=True)
        start=time.perf_counter()
        with (run/(name+'.log')).open('wb') as log:
            result=subprocess.run(command,stdout=log,stderr=log,env=env,startupinfo=startup,timeout=2400 if name=='full-scene-regression' else 1200)
        row.update(exitCode=result.returncode,seconds=time.perf_counter()-start,status='passed' if result.returncode==0 else 'failed')
        assert result.returncode==0,name
        print('DONE',name,row['seconds'],flush=True)
    report['status']='passed'
finally:(run/'remaining-status.json').write_text(json.dumps(report,indent=2)+'\n')
