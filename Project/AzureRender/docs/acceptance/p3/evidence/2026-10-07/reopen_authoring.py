import json,hashlib,subprocess,sys
from pathlib import Path
root=Path(__file__).resolve().parents[3];work=root/'build/evolution/p3';install=root/'build/ninja-msvc-release/release-gate/install-moved'
sys.path.insert(0,str(root/'tools'));from build_game import verify_package
from run_playable_long_run import isolated_environment
out=work/'authoring-reopened';out.mkdir(exist_ok=False)
game=work/'authoring/game';level=game/'assets/exploration.azurelevel';before=level.read_bytes()
actions=[{'frame':1,'command':'select','id':'hero:body'},
 {'frame':2,'command':'component-field','type':'azure.transform','field':'translation','value':[0,0,4]},
 {'frame':3,'command':'undo'},{'frame':4,'command':'play'},{'frame':14,'command':'stop'},{'frame':15,'command':'save'}]
(out/'actions.json').write_text(json.dumps(actions),encoding='utf-8');report=out/'runtime.json'
startup=subprocess.STARTUPINFO();startup.dwFlags|=subprocess.STARTF_USESHOWWINDOW;startup.wShowWindow=0
r=subprocess.run([str(install/'bin/AzureRender.exe'),'--editor-project',str(game/'project.azureproject'),
 '--editor-actions',str(out/'actions.json'),'--runtime-report',str(report),'--fixed-frame-step','--smoke-frames','24'],
 cwd=out,stdout=subprocess.PIPE,stderr=subprocess.PIPE,encoding='utf-8',errors='replace',timeout=180,startupinfo=startup)
(out/'stdout.log').write_text(r.stdout,encoding='utf-8');(out/'stderr.log').write_text(r.stderr,encoding='utf-8')
assert r.returncode==0,r.stdout+r.stderr
x=json.loads(report.read_text());assert all(a['passed'] for a in x['editorActions']) and x['editStateRestored'] and not x['editorPlaying']
assert level.read_bytes()==before,'Reopen, undo or Play/Stop changed the saved scene'
assert not x['scriptErrors'] and not x['presentationErrors']
assert 'Allocator after unload: buffers=0 images=0' in r.stdout and 'VUID-' not in r.stdout+r.stderr
package=work/'authoring/package';verify_package(package)
r=subprocess.run([str(package/'bin/AzurePlayer.exe'),'--project',str(package/'game/project.azureproject'),'--check-project'],
 cwd=out,env=isolated_environment(),capture_output=True,encoding='utf-8',errors='replace',timeout=120,startupinfo=startup)
(out/'package-validation.log').write_text(r.stdout+r.stderr,encoding='utf-8');assert r.returncode==0,r.stdout+r.stderr
result={'status':'passed','separateEditorProcess':True,'savedSceneSha256':hashlib.sha256(before).hexdigest(),
 'undoRestored':True,'playStopRestored':True,'independentPackageValidated':True,'physicalKeyboardMouse':'not performed'}
(out/'summary.json').write_text(json.dumps(result,indent=2)+'\n');print(json.dumps(result),flush=True)
