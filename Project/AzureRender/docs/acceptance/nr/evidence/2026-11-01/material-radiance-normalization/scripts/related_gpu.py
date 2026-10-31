import hashlib,json,os,subprocess,sys,time
from pathlib import Path
root=Path.cwd();run=Path(__file__).resolve().parent
layer_args=[arg for case in ['base','highlight','independent-samplers','scroll','document-mask','view-near','view-mask'] for arg in ['--case',case]]
env=os.environ.copy();env['DOTNET_ROOT']='D:/Tools/dotnet9';env['PYTHONUTF8']='1'
initial=json.loads((run/'related-gpu-initial-summary.json').read_text())
report={'status':'running','scope':'Targeted union-tail compatibility, eyes and hair in both configurations; complete NPR matrix in Release. Full repository regression remains separate.','checks':[row for row in initial['checks'] if row.get('status')=='passed']}
try:
    for cfg,validation in [('release','disabled'),('debug','enabled')]:
        player=root/('build/ninja-msvc-'+cfg)/'AzurePlayer.exe';digest=hashlib.sha256(player.read_bytes()).hexdigest()
        jobs=[('layers','test_unlit_texture_layers_gpu.py',layer_args+['--expected-validation',validation]),
              ('eye','test_eye_npr_gpu.py',['--expected-validation',validation]),
              ('hair','test_projected_hair_gpu.py',['--require-validation'] if cfg=='debug' else []),
              ('uv-distortion','test_vertex_color_gpu.py',['--case','uv-distortion','--expected-validation',validation])]
        if cfg=='release':jobs.append(('npr','test_npr_illumination_gpu.py',['--expected-validation',validation]))
        for name,script,args in jobs:
            previous=next((row for row in report['checks'] if row['configuration']==cfg and row['name']==name and row.get('status')=='passed'),None)
            if previous is not None:
                assert previous['programSha256']==digest
                print('REUSED',cfg,name,flush=True);continue
            output=run/'related-gpu-final'/cfg/name;output.parent.mkdir(parents=True,exist_ok=True)
            command=[sys.executable,'-X','utf8',str(root/'tools'/script),'--executable',str(player),'--output',str(output),*args]
            row={'configuration':cfg,'name':name,'command':command,'programSha256':digest};report['checks'].append(row)
            print('START',cfg,name,flush=True);start=time.perf_counter()
            with (output.parent/(name+'.log')).open('wb') as log:result=subprocess.run(command,stdout=log,stderr=log,env=env,timeout=900)
            row.update(exitCode=result.returncode,seconds=time.perf_counter()-start);assert result.returncode==0,row
            summary=json.loads((output/'summary.json').read_text());assert summary['status']=='passed',row
            row.update(status='passed',summary=summary)
            assert hashlib.sha256(player.read_bytes()).hexdigest()==digest
            print('DONE',cfg,name,row['seconds'],flush=True)
            (run/'related-gpu-summary.json').write_text(json.dumps(report,indent=2)+'\n')
    report['status']='passed'
except Exception as error:report.update(status='failed',failure=str(error));raise
finally:(run/'related-gpu-summary.json').write_text(json.dumps(report,indent=2)+'\n')
