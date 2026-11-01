import hashlib,json,subprocess
from pathlib import Path
root=Path.cwd();run=Path(__file__).resolve().parent;prior=root/'build/runs/20261101-043757-material-display-transform-93d6e0';sha=lambda p:hashlib.sha256(p.read_bytes()).hexdigest()
records={}
for cfg,name in [('release','release-final-build.json'),('debug','debug-build.json')]:
 build=json.loads((prior/name).read_text());assert build['status']=='built-contract-tests-passed'
 for path,digest in build['sourceFiles'].items():assert hashlib.sha256((root/path).read_bytes().replace(b'\r\n',b'\n')).hexdigest()==digest,path
 directory=root/('build/ninja-msvc-'+cfg)
 for name,digest in build['programs'].items():assert sha(directory/(name+'.exe'))==digest,name
 assert {p.name:sha(p) for p in (directory/'shaders').glob('*.spv')}==build['shaders']
 records[cfg]=dict(sourceFileCount=len(build['sourceFiles']),sourceFiles=build['sourceFiles'],programs=build['programs'],shaders=build['shaders'])
report=dict(status='passed',sourceCommit=subprocess.check_output(['git','rev-parse','HEAD'],text=True).strip(),scope='Project composition changes only. Actual programs and all shaders remain byte-identical to the verified display-transform build.',configurations=records)
(run/'build-reuse.json').write_bytes((json.dumps(report,indent=2)+'\n').encode());print('Build reuse verified: two configurations,',len(build['sourceFiles']),'compiled source files')
