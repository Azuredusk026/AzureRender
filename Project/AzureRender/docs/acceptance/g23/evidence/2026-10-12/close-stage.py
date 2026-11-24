from pathlib import Path
import difflib,gzip,hashlib,json,shutil,subprocess,sys
root=Path.cwd().resolve();out=root/'build/g23-compressed-audio-2026-10-12';dest=root/'docs/acceptance/g23/evidence/2026-10-12'
for name,count in [('debug-final-cpu',174),('release-cpu',174),('debug-gpu',9),('release-gpu',9)]:
 assert f'100% tests passed, 0 tests failed out of {count}' in (out/(name+'.log')).read_text(encoding='utf-8'),name
sys.path.insert(0,str(root/'tools'));from build_provenance import verify
proofs={c:json.loads((root/('build/ninja-msvc-'+c.lower())/'build-provenance.json').read_text(encoding='utf-8')) for c in ('Debug','Release')}
for proof in proofs.values():verify(root,proof)
assert proofs['Debug']['source']==proofs['Release']['source']
delivery=json.loads((out/'delivery-summary.json').read_text(encoding='utf-8'));assert delivery['passed'] and len(delivery['rows'])==4
assert json.loads((out/'ui-final/summary.json').read_text(encoding='utf-8'))['passed']
for file in out.glob('*.log'):(dest/(file.name+'.gz')).write_bytes(gzip.compress(file.read_bytes(),mtime=0))
for file in out.glob('*.py'):shutil.copy2(file,dest/file.name)
for folder in ('ui-final',):
 for file in (out/folder).rglob('*'):
  if file.is_file() and (file.suffix in ('.json','.log') or file.name=='frame_000099.png'):
   target=dest/folder/file.relative_to(out/folder);target.parent.mkdir(parents=True,exist_ok=True);shutil.copy2(file,target)
for c,proof in proofs.items():(dest/(c.lower()+'-provenance.json.gz')).write_bytes(gzip.compress(json.dumps(proof,indent=2).encode(),mtime=0))
shutil.copy2(out/'delivery-summary.json',dest/'delivery-summary.json')
(dest/'authoring-summary.json').write_bytes((out/'Sources unavailable/authoring-summary.json').read_bytes())
(dest/'install-manifest.json.gz').write_bytes(gzip.compress((out/'install-manifest.json').read_bytes(),mtime=0))
for device in ('nvidia','intel'):
 for workflow in ('opus','m4a'):
  name=device+'-'+workflow;shutil.copy2(out/(name+'-runtime.json'),dest/(name+'-runtime.json'))
  for file in (out/(name+'-capture')).glob('*'):
   if file.is_file():shutil.copy2(file,dest/(name+'-'+file.name))
for workflow in ('opus','m4a'):(dest/(workflow+'-package-manifest.json.gz')).write_bytes(gzip.compress((out/('Moved '+workflow)/'game_manifest.json').read_bytes(),mtime=0))
manifest=dict(schemaVersion=1,phase='G23',automaticComplete=True,finalAcceptancePhase='P8',sourceSnapshotSha256=hashlib.sha256(json.dumps(proofs['Release']['source'],sort_keys=True).encode()).hexdigest(),cpu={'Debug':174,'Release':174},gpuRegression={'Debug':9,'Release':9},emptyProjects=2,independentPackages=2,deviceWorkflowCombinations=4,manualAcceptance='DeferredToP8',installedFileCount=delivery['fileCount'],compressedAudioTestSha256=hashlib.sha256((root/'tests/CompressedAudioFormatsTests.cpp').read_bytes()).hexdigest(),compressedAudioTestBinaries={c:hashlib.sha256((root/('build/ninja-msvc-'+c)/'AzureCompressedAudioFormatsTests.exe').read_bytes()).hexdigest() for c in ('debug','release')},nativeAac={'authoredFrames':48000,'decodedFrames':48128,'longDecodedFrames':1488896},files=[])
for p in sorted(dest.rglob('*')):
 if p.is_file() and p!=dest/'manifest.json':manifest['files'].append({'path':p.relative_to(dest).as_posix(),'sha256':hashlib.sha256(p.read_bytes()).hexdigest(),'bytes':p.stat().st_size})
(dest/'manifest.json').write_text(json.dumps(manifest,indent=2)+'\n',encoding='utf-8')
baseline=json.loads((out/'stage-baseline.json').read_text(encoding='utf-8'));name='CMakeLists.txt';path='Project/AzureRender/'+name;patch=''.join(difflib.unified_diff(baseline[name]['work'].splitlines(keepends=True),(root/name).read_text(encoding='utf-8').splitlines(keepends=True),fromfile='a/'+path,tofile='b/'+path));subprocess.run(['git','apply','--cached','--check','-'],input=patch.encode(),check=True);subprocess.run(['git','apply','--cached','-'],input=patch.encode(),check=True)
files=['vcpkg.json','src/assets/audio/AudioCodec.cpp','src/assets/audio/AudioCodec.hpp','src/assets/audio/AudioEncoder.cpp','src/assets/audio/AudioFormatAdapter.cpp','src/assets/audio/CompressedAudioAdapter.cpp','src/assets/audio/CompressedAudioAdapter.hpp','src/runtime/AudioRuntime.cpp','tests/CompressedAudioFormatsTests.cpp','tests/EditorAssetWorkflowTests.cpp','tools/test_compressed_audio_workflows.py','docs/runtime/audio-format-import.md','docs/runtime/asset-exchange.md','docs/runtime/compressed-audio.md','docs/licenses/compressed-audio-source.json','docs/licenses/compressed-audio','docs/plans/2026-10-12-compressed-audio.md','docs/acceptance/g23/2026-10-12.md','docs/plans/asset-format-manifest.json','docs/plans/hazel-fermion-manifest.json','docs/plans/2026-10-11-next-development.md','docs/plans/2026-10-11-asset-format-expansion.md']
subprocess.run(['git','add','--',*files],check=True);subprocess.run(['git','add','-f','--',str(dest.parent)],check=True)
for row in manifest['files']:
 blob=subprocess.check_output(['git','show',':Project/AzureRender/'+dest.relative_to(root).as_posix()+'/'+row['path']]);assert hashlib.sha256(blob).hexdigest()==row['sha256'],row['path']
for name in ('opus','opusfile'):
 p=root/'docs/licenses/compressed-audio'/(name+'-LICENSE.txt');blob=subprocess.check_output(['git','show',':Project/AzureRender/'+p.relative_to(root).as_posix()]);assert blob==p.read_bytes(),name
print('G23 exact stage and',len(manifest['files']),'raw blobs verified; source',manifest['sourceSnapshotSha256'])
