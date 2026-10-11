from pathlib import Path
import difflib,gzip,hashlib,json,shutil,subprocess,sys
root=Path.cwd().resolve();out=root/'build/g21-dependency-evaluation-2026-10-12';dest=root/'docs/acceptance/g21/evidence/2026-10-12'
for config in ('debug','release'):
    assert '100% tests passed, 0 tests failed out of 170' in (out/(config+'-cpu.log')).read_text(encoding='utf-8')
    assert '100% tests passed, 0 tests failed out of 4' in (out/(config+'-gpu.log')).read_text(encoding='utf-8')
sys.path.insert(0,str(root/'tools'));from build_provenance import verify
proofs={config:json.loads((root/('build/ninja-msvc-'+config.lower())/'build-provenance.json').read_text(encoding='utf-8')) for config in ('Debug','Release')}
for proof in proofs.values():verify(root,proof)
assert proofs['Debug']['source']==proofs['Release']['source']
delivery=json.loads((out/'delivery-summary.json').read_text(encoding='utf-8'));assert delivery['passed']
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
for name in ('delivery-summary.json','module-boundaries.json','module-boundaries-release.json','user-source-preservation.json'):
    shutil.copy2(out/name,dest/name)
for file in out.glob('*-runtime.json'):shutil.copy2(file,dest/file.name)
if (out/'install-manifest.json').exists():(dest/'install-manifest.json.gz').write_bytes(gzip.compress((out/'install-manifest.json').read_bytes(),mtime=0))
for config,proof in proofs.items():(dest/(config.lower()+'-provenance.json.gz')).write_bytes(gzip.compress(json.dumps(proof,indent=2).encode(),mtime=0))
for name in ('nvidia-capture','intel-capture'):shutil.copytree(out/name,dest/name,dirs_exist_ok=True)
shutil.copytree(out/'roundtrip-outputs',dest/'roundtrip-outputs',dirs_exist_ok=True)
(dest/'exchange-package-manifest.json.gz').write_bytes(gzip.compress((out/'Moved exchange game/game_manifest.json').read_bytes(),mtime=0))
(dest.parent/'.gitattributes').write_text('** -text -whitespace\n',encoding='utf-8')
manifest={'schemaVersion':1,'phase':'G21','automaticComplete':True,'finalAcceptancePhase':'P8','sourceSnapshotSha256':hashlib.sha256(json.dumps(proofs['Release']['source'],sort_keys=True).encode()).hexdigest(),'sourceScope':'Complete working-tree snapshot with preserved user edits','cpu':{'Debug':170,'Release':170},'gpuRegression':{'Debug':4,'Release':4},'writers':16,'devices':['NVIDIA','Intel'],'installedFileCount':delivery['fileCount'],'packageFileCount':delivery['packageFiles'],'manualAcceptance':'Deferred to P8','files':[]}
for file in sorted(dest.rglob('*')):
    if file.is_file() and file.name!='manifest.json':manifest['files'].append({'path':file.relative_to(dest).as_posix(),'sha256':hashlib.sha256(file.read_bytes()).hexdigest(),'bytes':file.stat().st_size})
(dest/'manifest.json').write_text(json.dumps(manifest,indent=2)+'\n',encoding='utf-8')
baseline=json.loads((out/'stage-baseline.json').read_text(encoding='utf-8'))
for name in ('CMakeLists.txt','vcpkg.json','src/assets/AssetTransfer.cpp','src/assets/AssetTransfer.hpp','src/assets/AssetFormatRegistry.cpp','src/assets/AssetFormatRegistry.hpp','src/assets/model/ModelFormatAdapter.cpp','src/assets/model/ModelFormatAdapter.hpp','src/assets/image/ImageFormatAdapter.cpp','src/assets/audio/AudioFormatAdapter.cpp','tests/EditorAssetWorkflowTests.cpp','tools/build_provenance.py','tools/test_build_provenance.py'):
    path='Project/AzureRender/'+name;old=baseline[name]['work'];new=(root/name).read_text(encoding='utf-8')
    patch=''.join(difflib.unified_diff(old.splitlines(keepends=True),new.splitlines(keepends=True),fromfile='a/'+path,tofile='b/'+path))
    if patch:
        subprocess.run(['git','-C',str(root.parents[1]),'apply','--cached','--check','-'],input=patch.encode(),check=True)
        subprocess.run(['git','-C',str(root.parents[1]),'apply','--cached','-'],input=patch.encode(),check=True)
paths=['src/assets/AssetExportJob.hpp','src/assets/AssetExportJob.cpp','src/assets/model/ModelExportScene.hpp','src/assets/image/ImageEncoder.hpp','src/assets/image/ImageEncoder.cpp','src/assets/audio/AudioEncoder.hpp','src/assets/audio/AudioEncoder.cpp','tests/AssetRoundTripTests.cpp','tools/asset-exchange/main.cpp','tools/test_asset_exchange.py','docs/runtime/asset-exchange.md','docs/runtime/asset-formats.md','docs/acceptance/g21/2026-10-12.md','docs/plans/asset-format-manifest.json','docs/plans/hazel-fermion-manifest.json','docs/plans/2026-10-11-asset-format-expansion.md','docs/plans/2026-10-11-next-development.md','docs/plans/2026-10-12-asset-export-implementation.md']
subprocess.run(['git','add','--',*paths],check=True);subprocess.run(['git','add','-f','--',str(dest.parent)],check=True)
for entry in manifest['files']:
    blob=subprocess.check_output(['git','show',':Project/AzureRender/'+dest.relative_to(root).as_posix()+'/'+entry['path']]);assert hashlib.sha256(blob).hexdigest()==entry['sha256'],entry['path']
print('Verified',len(manifest['files']),'raw Git evidence blobs and exact G21 staging; source',manifest['sourceSnapshotSha256'])
