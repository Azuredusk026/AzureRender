from pathlib import Path
import gzip,hashlib,json,shutil,subprocess,sys
root=Path.cwd();out=root/'build/g19-dependency-evaluation-2026-10-12';dest=root/'docs/acceptance/g19/evidence/2026-10-12'
for name,count in [('debug-cpu.log',166),('release-cpu.log',166),('debug-gpu.log',13),('release-gpu.log',13)]:assert f'100% tests passed, 0 tests failed out of {count}' in (out/name).read_text(encoding='utf-8'),name
for config in ('Debug','Release'):
    report=json.loads((out/'image-render'/config/'summary.json').read_text(encoding='utf-8'));assert report['passed'] and len(report['cases'])==22
    assert report['losslessMaterialGpuPixelEquality'] and {r['device'] for r in report['cases']}=={'nvidia','intel'}
delivery=json.loads((out/'delivery-summary.json').read_text(encoding='utf-8'));assert delivery['passed'] and delivery['fileCount']==8566
sys.path.insert(0,str(root/'tools'));from build_provenance import verify
proofs={config:json.loads((root/('build/ninja-msvc-'+config.lower())/'build-provenance.json').read_text(encoding='utf-8')) for config in ('Debug','Release')}
for proof in proofs.values():verify(root,proof)
assert proofs['Debug']['source']==proofs['Release']['source']
for name,hash in delivery['installedBinaries'].items():assert hashlib.sha256((root/'build/ninja-msvc-release'/name).read_bytes()).hexdigest()==hash
original=json.loads((root/'build/automatic-resume-2026-10-11/workspace-baseline.json').read_text(encoding='utf-8'))
previous=json.loads((root/'build/g18-dependency-evaluation-2026-10-12/runtime-utf8-baseline.json').read_text(encoding='utf-8'))
names=['src/app/AzureRenderApp.cpp','src/app/AzureRenderApp.hpp','src/app/AzureRenderCapture.cpp','src/app/AzureRenderSupport.cpp','src/editor/commands/EditService.cpp','src/editor/commands/EditService.hpp','src/runtime/EngineSettings.cpp','tests/EngineSettingsTests.cpp','tests/PipelineInspectionTests.cpp','tools/run_playable_long_run.py','tools/run_playable_performance.py','tools/test_level_player.py','tools/test_validation_protocol.py']
for name in names:
    if name==names[0]:assert (root/name).read_text(encoding='utf-8')==previous[name]['result']
    else:assert hashlib.sha256((root/name).read_bytes()).hexdigest()==original['Project/AzureRender/'+name],name
(out/'user-source-preservation.json').write_text(json.dumps({'passed':True,'unchangedSinceG18':names,'initialUserEditsPreserved':True},indent=2)+'\n',encoding='utf-8')
dest.mkdir(parents=True,exist_ok=True)
for file in out.glob('*.log'):(dest/(file.name+'.gz')).write_bytes(gzip.compress(file.read_bytes(),mtime=0))
for file in out.glob('*.py'):shutil.copy2(file,dest/file.name)
for name in ('delivery-summary.json','dependency-evaluation.json','module-boundaries.json','user-source-preservation.json','install-final-manifest.json'):shutil.copy2(out/name,dest/name)
for config,proof in proofs.items():(dest/(config.lower()+'-provenance.json.gz')).write_bytes(gzip.compress(json.dumps(proof,indent=2).encode(),mtime=0))
for name in ('image-fixtures','image-render','lossless-repeat'):shutil.copytree(out/name,dest/name,dirs_exist_ok=True)
attributes=dest.parent/'.gitattributes';attributes.write_text('** -text -whitespace\n',encoding='utf-8')
manifest={'schemaVersion':1,'phase':'G19','automaticComplete':True,'finalAcceptancePhase':'P8','sourceSnapshotSha256':hashlib.sha256(json.dumps(proofs['Release']['source'],sort_keys=True).encode()).hexdigest(),
    'sourceScope':'Complete working-tree snapshot, including preserved user edits','cpu':{'Debug':166,'Release':166},'gpuRegression':{'Debug':13,'Release':13},'newImageGpuCases':{'Debug':22,'Release':22},
    'precision':['UInt8','UInt16','Float32'],'deviceScope':['NVIDIA','Intel'],'installedFileCount':delivery['fileCount'],'firstFrameLosslessMaterialPixelsEqual':True,
    'observations':['Intel full-frame first comparison differed by two one-level floor pixels; identical PNG repeat also varied','Three first-frame material ROI comparisons matched exactly; later frame diagnostics had at most two-level differences','Original visual and performance budgets remain effective'],
    'manualAcceptance':'Deferred to P8','files':[]}
for file in sorted(dest.rglob('*')):
    if file.is_file() and file.name!='manifest.json':manifest['files'].append({'path':file.relative_to(dest).as_posix(),'sha256':hashlib.sha256(file.read_bytes()).hexdigest(),'bytes':file.stat().st_size})
(dest/'manifest.json').write_text(json.dumps(manifest,indent=2)+'\n',encoding='utf-8')
subprocess.run(['git','add','-f','--',str(dest.parent)],check=True)
for entry in manifest['files']:
    blob=subprocess.check_output(['git','show',':Project/AzureRender/'+dest.relative_to(root).as_posix()+'/'+entry['path']]);assert hashlib.sha256(blob).hexdigest()==entry['sha256'],entry['path']
print('Archived and verified',len(manifest['files']),'Git evidence blobs; source',manifest['sourceSnapshotSha256'])
