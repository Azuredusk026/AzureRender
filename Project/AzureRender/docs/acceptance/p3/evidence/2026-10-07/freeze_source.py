import json,hashlib,subprocess,sys,tarfile
from pathlib import Path
root=Path(__file__).resolve().parents[3];sys.path.insert(0,str(root/'tools'))
from build_provenance import inputs,verify
work=root/'build/evolution/p3';output=work/'source-snapshot';output.mkdir(exist_ok=False)
configurations={}
for config in ('debug','release'):
 m=json.loads((root/('build/ninja-msvc-'+config)/'build-provenance.json').read_text());verify(root,m);configurations[config]=m
assert configurations['debug']['source']==configurations['release']['source']
frozen=inputs(root);allowed={'src','shaders','tools','schemas','assets_public','cmake','managed','third_party','tests'}
files=subprocess.check_output(['git','ls-files','--cached','--others','--exclude-standard'],cwd=root).decode('utf-8').splitlines()
files=sorted(set(n for n in files if n.split('/')[0] in allowed or n in {'CMakeLists.txt','CMakePresets.json','vcpkg.json','vcpkg-configuration.json','LICENSE','LICENSE.txt','requirements-docs.txt','mkdocs.yml','docs/plans/engine-evolution-manifest.json'}))
files.append('docs/plans/engine-evolution-manifest.json');records={}
for name in sorted(set(files)):
 source=root/name
 if not source.is_file():continue
 data=source.read_bytes();p=output/name;p.parent.mkdir(parents=True,exist_ok=True);p.write_bytes(data)
 h=hashlib.sha256(data).hexdigest();records[name]={'sha256':h,'bytes':len(data)}
 if name in frozen:assert frozen[name]==h,name
assert inputs(root)==frozen
assert all(n in records and records[n]['sha256']==h for n,h in frozen.items())
receipt={'schemaVersion':1,'kind':'exact-working-tree-source-snapshot','parentCommit':configurations['release']['commit'],
 'files':records,'buildInputs':frozen,'scope':'engine source, tests, tools, shaders, public assets, schemas, vendored sources and licenses; active documentation delivered in engine install',
 'historicalScope':'This archive preserves current delivery bytes; historical original working-tree bytes have a separate audit.'}
(output/'source-manifest.json').write_text(json.dumps(receipt,ensure_ascii=False,indent=2)+'\n',encoding='utf-8')
for n,r in records.items():assert hashlib.sha256((output/n).read_bytes()).hexdigest()==r['sha256']
package=work/'packages/AzureEngine-P3-source.tar.gz';package.parent.mkdir(exist_ok=True)
with tarfile.open(package,'w:gz') as archive:archive.add(output,arcname='AzureEngine-P3-source')
with tarfile.open(package,'r:gz') as archive:
 for n,r in records.items():assert hashlib.sha256(archive.extractfile('AzureEngine-P3-source/'+n).read()).hexdigest()==r['sha256']
result={'status':'passed','files':len(records),'productionInputs':len(frozen),'archive':str(package),'sha256':hashlib.sha256(package.read_bytes()).hexdigest(),'bytes':package.stat().st_size}
(work/'source-snapshot.json').write_text(json.dumps(result,indent=2)+'\n');print(json.dumps(result),flush=True)
