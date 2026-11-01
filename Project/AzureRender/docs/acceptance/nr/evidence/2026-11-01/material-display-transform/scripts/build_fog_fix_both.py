import json,subprocess,sys
from pathlib import Path
root=Path.cwd();run=Path(__file__).resolve().parent
for cfg,name in [('release','build_release_final.py'),('debug','build_debug.py')]:
    print('BUILD',cfg,flush=True)
    with (run/('build-fog-fix-'+cfg+'-driver.log')).open('wb') as log:subprocess.run([sys.executable,'-X','utf8',str(run/name)],cwd=root,stdout=log,stderr=log,check=True)
    before=json.loads((run/('pre-fog-fix-'+('release-final-build.json' if cfg=='release' else 'debug-build.json'))).read_text())
    after=json.loads((run/('release-final-build.json' if cfg=='release' else 'debug-build.json')).read_text())
    changes=[key for key in after['sourceFiles'] if before['sourceFiles'].get(key)!=after['sourceFiles'][key]]
    shaders=[key for key in after['shaders'] if before['shaders'].get(key)!=after['shaders'][key]]
    (run/('fog-fix-build-delta-'+cfg+'.json')).write_text(json.dumps(dict(sourceChanges=changes,shaderChanges=shaders,programsIdentical=before['programs']==after['programs']),indent=2)+'\n')
    print('DONE',cfg,changes,shaders,flush=True)
