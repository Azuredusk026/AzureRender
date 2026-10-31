import hashlib,json,subprocess
from pathlib import Path
import numpy as np
root=Path.cwd();run=Path(__file__).resolve().parent;coverage=json.loads((run/'gi-coverage.json').read_text())
points=run/'gi-points.txt';points.write_text(''.join(' '.join(str(v) for v in c['world'])+'\n' for c in coverage['cases']))
program=root/'build/ninja-msvc-release/AzureIrradianceClipmapGpuTests.exe';shader=root/'build/ninja-msvc-release/shaders/irradiance_clipmap_test.comp.spv';field=root/coverage['source']
command=[str(program),str(shader),str(field),str(points),'--sample-only']
result=subprocess.run(command,capture_output=True,timeout=60);(run/'gi-query.stdout').write_bytes(result.stdout);(run/'gi-query.stderr').write_bytes(result.stderr)
assert result.returncode==0,result.stderr.decode(errors='replace')
lines=[line for line in result.stdout.decode().splitlines() if len(line.split())==24]
assert len(lines)==len(coverage['cases']),(len(lines),len(coverage['cases']))
cases=[]
for point,line in zip(coverage['cases'],lines):
    values=np.array([float(v) for v in line.split()]);assert np.isfinite(values).all();coefficients=values[:12].reshape(3,4)
    normals={'up':[0,1,0],'front-negative-z':[0,0,-1],'front-positive-z':[0,0,1],'positive-x':[1,0,0],'negative-x':[-1,0,0]}
    cases.append({'name':point['name'],'world':point['world'],'coefficients':coefficients.tolist(),'incidentByNormal':{name:np.maximum(coefficients@np.r_[normal,1],0).tolist() for name,normal in normals.items()},'alphaLanes':values[12:].tolist()})
report={'status':'finite-gpu-query','scope':'Actual clipmap compute consumer; incident values before material response, ambient weight, display exposure and color grading. Raw-query mode has no source-oracle image acceptance.','command':command,'programSha256':hashlib.sha256(program.read_bytes()).hexdigest(),'shaderSha256':hashlib.sha256(shader.read_bytes()).hexdigest(),'fieldSha256':hashlib.sha256(field.read_bytes()).hexdigest(),'cases':cases}
(run/'gi-query.json').write_text(json.dumps(report,indent=2)+'\n');print(json.dumps({'cases':len(cases),'incident':{c['name']:c['incidentByNormal']['front-negative-z'] for c in cases}},indent=2))
