from pathlib import Path
import hashlib,json,os,shutil,subprocess,sys,time
root=Path.cwd();r=Path(__file__).resolve().parent;source=root/'build/runs/20261031-183314-full-scene-playable-validation-d5257e/project';target=r/'validation-probe-project';assert not target.exists()
def copy(a,b):
 p=Path(a)
 if p.suffix.lower() in {'.azureproject','.azurelevel','.azurematerial','.json','.lua','.azmeta','.md','.txt'}:shutil.copy2(a,b)
 else:os.link(a,b)
 return str(b)
shutil.copytree(source,target,ignore=shutil.ignore_patterns('.azure','__pycache__'),copy_function=copy)
sys.path.insert(0,str(root/'tools'));import build_game
player=root/'build/ninja-msvc-release/AzurePlayer.exe';project=target/'playable.azureproject';sha=lambda p:hashlib.sha256(p.read_bytes()).hexdigest();report=dict(status='running',toolSha256=sha(root/'tools/build_game.py'),programSha256=sha(player),projectSha256=sha(project),levelSha256=sha(target/'scene/playable.azurelevel'),defaultTimeoutSeconds=60,scope='Actual unmodified build_game.validate_project on a separate copy of the closed project; no package publication')
start=time.perf_counter()
try:
 build_game.validate_project(player,project);report.update(status='passed-within-existing-budget')
except subprocess.TimeoutExpired as error:
 report.update(status='observed-default-budget-timeout',command=list(map(str,error.cmd)),timeoutSeconds=error.timeout)
 for name,value in [('stdout',error.stdout),('stderr',error.stderr)]:
  (r/('validation-budget-'+name+'.log')).write_bytes(value if isinstance(value,bytes) else (value or '').encode())
except Exception as error:
 report.update(status='failed-other',error=repr(error));raise
finally:
 report['elapsedSeconds']=time.perf_counter()-start;(r/'validation-budget-probe.json').write_bytes((json.dumps(report,indent=2)+'\n').encode());print(json.dumps(report,indent=2))
