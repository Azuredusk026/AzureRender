from pathlib import Path
import subprocess,sys,json,hashlib
root=Path.cwd();work=root/'build/p4'
summary_path=max((work/'delivery').glob('run-*/summary.json'),key=lambda p:p.stat().st_mtime)
summary=json.loads(summary_path.read_text(encoding='utf-8'));moved=Path(summary['engine']);manifest=moved/'install_manifest.json'
commands=[['cmake','--install',str(root/'build/ninja-msvc-release'),'--config','Release','--prefix',str(moved)],
 ['cmake','-DINSTALL_DIR='+str(moved),'-DOUTPUT_FILE='+str(manifest),'-P',str(root/'tools/write_install_manifest.cmake')],
 ['cmake','-DINSTALL_DIR='+str(moved),'-DMANIFEST_FILE='+str(manifest),'-P',str(root/'tools/verify_install_manifest.cmake')]]
with (work/'document-delivery.log').open('w',encoding='utf-8') as output:
 for command in commands:subprocess.run(command,stdout=output,stderr=subprocess.STDOUT,check=True)
 # Verify the installed documentation and raw evidence byte for byte.
 files=[Path('README.md')]
 resources=moved/'share/AzureRender'
 pairs=[(root/'README.md',resources/'README.md')]
 for relative in ['docs/acceptance/p4','docs/plans/2026-10-08-editor-acceptance.md','docs/plans/2026-10-07-editor-usability.md','docs/plans/azure-engine-plan.md','docs/plans/editor-usability-manifest.json','docs/runtime/edit-operations.md','docs/tutorials/editor-interaction.md']:
  source=root/relative
  paths=source.rglob('*') if source.is_dir() else [source]
  for path in paths:
   if path.is_file():pairs.append((path,resources/path.relative_to(root)))
 for source,target in pairs:
  assert source.read_bytes()==target.read_bytes(),str(target)
 # Executable hashes match the machine-tested build.
 for name in ['AzureRender.exe','AzurePlayer.exe']:
  assert (moved/'bin'/name).read_bytes()==(root/'build/ninja-msvc-release'/name).read_bytes(),name
 report=dict(passed=True,files=len(pairs),installManifestSha256=hashlib.sha256(manifest.read_bytes()).hexdigest(),runtimeProductsUnchanged=True,evidenceInstalled=True)
 (work/'document-delivery.json').write_text(json.dumps(report,indent=2),encoding='utf-8');print(json.dumps(report),flush=True)
