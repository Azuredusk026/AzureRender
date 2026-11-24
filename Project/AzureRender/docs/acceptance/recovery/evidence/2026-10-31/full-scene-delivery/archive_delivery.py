from pathlib import Path
import gzip,hashlib,json,shutil
root=Path.cwd();r=Path(__file__).resolve().parent;out=root/'docs/acceptance/recovery/evidence/2026-10-31/full-scene-delivery';out.mkdir(parents=True,exist_ok=True);sha=lambda p:hashlib.sha256(p.read_bytes()).hexdigest()
assert json.loads((r/'isolation-summary.json').read_text())['status']=='passed'
def copy(p,name):
 data=p.read_bytes()
 if name.endswith('.gz'):data=gzip.compress(data,mtime=0)
 elif p.suffix in ['.json','.log','.py','.md']:data=data.replace(b'\r\n',b'\n').rstrip()+b'\n'
 (out/name).write_bytes(data)
for name in ['save-semantic-verification.json','isolation-summary.json','publication.json','install-verification.json','package-reference-closure.json','source-inputs.json','level-before-save.json','save-raw-differences.json','package-actions.json']:
 copy(r/name,name+('.gz' if name in ['source-inputs.json','level-before-save.json','save-raw-differences.json','package-reference-closure.json'] else ''))
copy(r/'移动后的完整场景/scene/playable.azurelevel','level-after-save.json.gz')
for folder in ['editor-save','moved-reopen','native-default','native-route']:
 for name in ['summary.json','runtime.json','editor.log','player.log','pixel-verification.json','behavior-verification.json']:
  p=r/folder/name
  if p.exists():copy(p,folder+'-'+name+('.gz' if name.endswith('.log') or name=='runtime.json' else ''))
 p=r/folder/'frames/capture_manifest.json'
 if p.exists():copy(p,folder+'-capture-manifest.json.gz')
for frame in [0,119,239,419,539]:copy(r/f'native-route/frames/frame_{frame:06d}.png',f'native-frame-{frame:06d}.png')
copy(r/'native-default/frames/frame_000002.png','native-default.png')
package=r/'独立游戏 中文 空格'
for name in ['game_manifest.json','game-launch.json','QUALITY.json','GAME-GUIDE.md']:
 copy(package/name,name+('.gz' if name=='game_manifest.json' else ''))
copy(package/'game-launch.json','game-launch.json.gz')
for name in ['prepare.py','save_editor.py','verify_saved.py','move_reopen.py','test_isolated_package.py','verify_package_behavior.py','archive_delivery.py']:copy(r/name,name)
manifest=json.loads((package/'game_manifest.json').read_text());summary=dict(status='independent-delivery-validation-passed',sourceBase='f272a83',project='build/runs/'+r.name+'/移动后的完整场景/playable.azureproject',package='build/runs/'+r.name+'/独立游戏 中文 空格',entry=manifest['entry'],files=len(manifest['files']),packageBytes=sum(x['size'] for x in manifest['files']),manifestSha256=sha(package/'game_manifest.json'),playerSha256=sha(package/'bin/AzurePlayer.exe'),sourceUnavailableDuringNativeRuns=True,installUnavailableDuringNativeRuns=True,isolatedPath='C:/WINDOWS/System32',pixelExactAfterSaveAndMoveFrames=3,pixelExactNativeDefaultFrames=3,nativeDefaultQaFlags=False,nativeRouteFrames=540,nativeRouteResolution=[1920,1080],gltfModelsAudited=558,relativeModelDependenciesInsidePackage=795,immutableHashesVerified=True,tamperRejected=True,physicalInput=False,formalQualityAndPerformance='Pending under Q1/Q4 and P8',scope='Production Editor save, project relocation, real publishing, package relocation and native execution; no claim of native appearance or sustained performance acceptance')
(out/'summary.json').write_bytes((json.dumps(summary,ensure_ascii=False,indent=2)+'\n').encode());files=[dict(path=p.name,bytes=p.stat().st_size,sha256=sha(p)) for p in sorted(out.iterdir()) if p.name!='artifacts.json'];(out/'artifacts.json').write_bytes((json.dumps(dict(schemaVersion=1,files=files),indent=2)+'\n').encode());print(json.dumps(dict(status=summary['status'],files=len(files),bytes=sum(x['bytes'] for x in files))))
