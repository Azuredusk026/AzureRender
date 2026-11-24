import json,hashlib,tarfile,sys
from pathlib import Path
root=Path(__file__).resolve().parents[3];work=root/'build/evolution/p3';packages=work/'packages'
sys.path.insert(0,str(root/'tools'));from build_game import verify_package
from write_engine_contracts import verify_bundle
from build_provenance import verify
release=root/'build/ninja-msvc-release';proof=json.loads((release/'build-provenance.json').read_text());verify(root,proof)
final=json.loads((work/'final-package-result.json').read_text());engine=Path(final['package']);assert hashlib.sha256(engine.read_bytes()).hexdigest()==final['sha256']
rows=[{'kind':'engine','path':str(engine),'sha256':final['sha256'],'bytes':engine.stat().st_size},
 {'kind':'source-snapshot',**json.loads((work/'source-snapshot.json').read_text())}]
for kind,directory in [('exploration',release/'playable-package/Azure Exploration'),('scene-inspector',work/'inspector-package/Moved Scene Inspector')]:
 manifest=verify_package(directory);archive=packages/('AzureEngine-P3-'+kind+'.tar.gz')
 with tarfile.open(archive,'w:gz') as stream:stream.add(directory,arcname=directory.name)
 with tarfile.open(archive,'r:gz') as stream:
  for item in manifest['files']:
   data=stream.extractfile(directory.name+'/'+item['path']).read()
   expected=item.get('sha256',item.get('hash'))
   assert hashlib.sha256(data).hexdigest()==expected,item['path']
 rows.append({'kind':kind,'directory':str(directory),'path':str(archive),'sha256':hashlib.sha256(archive.read_bytes()).hexdigest(),
  'bytes':archive.stat().st_size,'manifestSha256':hashlib.sha256((directory/'game_manifest.json').read_bytes()).hexdigest()})
contract=release/'release-gate/install-moved/share/AzureRender/contracts';v=verify_bundle(contract)
assert v['coveredItems']==18 and len(v['files'])==42
# Preserve portable source and product identifiers next to the delivered archives.
portable={'schemaVersion':1,'configuration':'Release','parentCommit':proof['commit'],'source':proof['source'],
 'products':{Path(n).relative_to(release).as_posix():h for n,h in proof['products'].items()},'packages':rows,
 'sourceSnapshotPolicy':'exact original bytes retained in phase evidence','historicalByteAudit':'explicit limits in the coverage report'}
p=packages/'AzureEngine-P3-provenance.json';p.write_text(json.dumps(portable,indent=2)+'\n',encoding='utf-8')
(work/'delivery-packages.json').write_text(json.dumps({'status':'passed','packages':rows,'portableProvenance':str(p)},indent=2)+'\n')
print(json.dumps({'status':'passed','packages':len(rows),'interfaceFiles':len(v['files'])}),flush=True)
