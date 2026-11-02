import gzip,hashlib,json
from pathlib import Path
root=Path.cwd();run=Path(__file__).resolve().parent;source=root/'build/runs/20261101-101614-view-fog-reuse-814a54/release-build.json';d=json.loads(source.read_text());sha=lambda p:hashlib.sha256(p.read_bytes()).hexdigest()
for name,digest in d['sourceFiles'].items():assert hashlib.sha256((root/name).read_bytes().replace(b'\r\n',b'\n')).hexdigest()==digest,name
assert sha(root/'build/ninja-msvc-release/AzurePlayer.exe')==d['programs']['AzurePlayer'];assert {p.name:sha(p) for p in (root/'build/ninja-msvc-release/shaders').glob('*.spv')}==d['shaders'];(run/'control-build-reuse.json').write_bytes((json.dumps(dict(status='verified',sourceRecord=str(source),sourceRecordSha256=sha(source),sourceFiles=d['sourceFiles'],programs=d['programs'],shaders=d['shaders']),indent=2)+'\n').encode())
for name in ['tests/MaterialDocumentTests.cpp','src/assets/GltfLoader.hpp','src/assets/GltfLoader.cpp','src/assets/MaterialSurfaceProfile.hpp','shaders/mesh.frag','schemas/azure_render_material.schema.json']:
 p=root/name;q=run/'before'/Path(name+'.gz');q.parent.mkdir(parents=True,exist_ok=True);q.write_bytes(gzip.compress(p.read_bytes(),mtime=0))
print('Frozen 988 control source files and original implementation snapshots')
