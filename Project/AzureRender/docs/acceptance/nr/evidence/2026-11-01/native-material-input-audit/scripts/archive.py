import gzip,hashlib,json,shutil,subprocess
from pathlib import Path
root=Path.cwd();run=Path(__file__).resolve().parent;dest=root/'docs/acceptance/nr/evidence/2026-11-01/native-material-input-audit';dest.mkdir(parents=True,exist_ok=True)
sha=lambda raw:hashlib.sha256(raw).hexdigest()
def copy(source,name=None):
    target=dest/(name or source.name);target.parent.mkdir(parents=True,exist_ok=True)
    if source.suffix in ['.py','.json','.azurelevel','.azureproject','.azurematerial','.azmeta','.txt']:
        target.write_bytes(source.read_bytes().replace(b'\r\n',b'\n'))
    else:target.write_bytes(source.read_bytes())
def zipped(source,name):
    target=dest/name;target.parent.mkdir(parents=True,exist_ok=True);target.write_bytes(gzip.compress(source.read_bytes(),mtime=0))
for name in ['verification.json','audit.json','profile-differences.json','hair-native-pose.json','texture-audit.json','material-ab-comparison.json','gi-coverage.json','gi-query.json','material-ab-capture.json','material-ab-extra-capture.json','material-ab-inputs.json','material-ab-cases.json','material-ab-extra-cases.json']:copy(run/name)
for name in ['audit.py','compare_profiles.py','texture_audit.py','prepare_material_ab.py','prepare_rim_ab.py','capture_material_ab.py','compare_material_ab.py','gi_coverage.py','query_gi.py','verify.py','archive.py']:copy(run/name,'scripts/'+name)
for folder in ['material-capture','material-capture-2','material-capture-3']:
    for case in (run/folder).iterdir():
        if not case.is_dir():continue
        zipped(case/'player.log',folder+'/'+case.name+'/player.log.gz')
        capture=case/'frames/capture_manifest.json'
        if capture.exists():
            copy(capture,folder+'/'+case.name+'/capture_manifest.json')
            frames=sorted((case/'frames').glob('frame_*.png'));assert len(frames)==32
            (dest/folder/case.name/'frames-manifest.json').write_text(json.dumps([{'file':p.name,'sha256':sha(p.read_bytes())} for p in frames],indent=2)+'\n')
            copy(frames[-1],folder+'/'+case.name+'/'+frames[-1].name)
        if (case/'runtime.json').exists():copy(case/'runtime.json',folder+'/'+case.name+'/runtime.json')
for source in (run/'material-ab').glob('*.azureproject'):copy(source,'inputs/'+source.name)
for dirname in ['levels','profiles']:
    for source in (run/'material-ab'/dirname).iterdir():
        if source.is_file():copy(source,'inputs/'+dirname+'/'+source.name)
for source in run.glob('material-ab-*-head.png'):copy(source)
copy(run/'gi-points.txt')
for name in ['gi-query.stdout','gi-query.stderr']:
    source=run/name;zipped(source,name+'.gz');(dest/name).write_text(''.join(line.rstrip()+'\n' for line in source.read_text().splitlines()),encoding='utf-8',newline='\n')
for name in ['capture-driver.log','capture-driver-2.log','capture-driver-3.log','capture-driver-rim.log']:zipped(run/name,name+'.gz')
native=root/'build/runs/20261022-212213-native-binding-restoration-d39ea8/frame10401'
native_inputs=[native/name for name in ['e804-PS-cb2.bin','e804-skin-matrices.bin','e950-ps-cb2-binding.bin','frame-bone-comparison.json']]
for source in native_inputs:zipped(source,'native/'+source.name+'.gz')
copy(native/'e950-ps-cb2-binding-receipt.json','native/e950-ps-cb2-binding-receipt.json')
geometry_path=root/'build/runs/20261017-000956-endfield-native-3caf73/laevat-static/model.geometry.json'
geometry=json.loads(geometry_path.read_text());selected=[{'name':m['name'],'bones':m['bones']} for m in geometry['meshes'] if m['name'] in ['S_actor_laevat_face_01_lod0','S_actor_laevat_hair_01_lod0']]
(dest/'native/inverse-binds.json').write_text(json.dumps({'sourceSha256':sha(geometry_path.read_bytes()),'meshes':selected},indent=2)+'\n')
sources=native_inputs+[geometry_path,root/'tools/retarget_animation.py',root/'tools/endfield_native/capture_skin_frame.py',root/'build/ninja-msvc-release/AzurePlayer.exe',root/'build/ninja-msvc-release/AzureIrradianceClipmapGpuTests.exe',root/'build/ninja-msvc-release/shaders/irradiance_clipmap_test.comp.spv']
sources.extend([root/'build/runs/20261031-135416-character-portrait-recovery-0773ca/original-reference/project/native-character/outline-converted.glb',root/'build/runs/20261031-231334-receiver-directional-light-911ab7/project/content-character/hero-restored-inputs-v8.glb'])
(dest/'sources.json').write_text(json.dumps({'sourceCommit':subprocess.check_output(['git','rev-parse','HEAD'],text=True).strip(),'files':[{'path':p.relative_to(root).as_posix(),'bytes':p.stat().st_size,'sha256':sha(p.read_bytes())} for p in sources]},indent=2)+'\n')
manifest=root/'docs/development/artifact-manifest.json';document=json.loads(manifest.read_text());entry={'path':'runs/'+run.name,'pathBase':'build','purpose':'角色原生姿态、同条件材质对照及辐照覆盖核验。','retention':'相应画质门禁完成并归档后另行清理。','pinMarker':'.keep','primaryProject':'material-ab/reference.azureproject','evidence':'docs/acceptance/nr/native-material-input-audit.md'}
existing=next((v for v in document['preservedRuns'] if v['path']==entry['path']),None)
if existing is None:document['preservedRuns'].append(entry)
else:assert existing==entry
manifest.write_text(json.dumps(document,indent=2,ensure_ascii=False)+'\n',encoding='utf-8');(run/'.keep').write_text('Native material input audit; see docs/acceptance/nr/native-material-input-audit.md\n')
for path in dest.rglob('*'):
    if path.is_file() and path.suffix in ['.py','.json','.azureproject','.azurelevel','.azurematerial','.azmeta','.txt','.stdout','.stderr']:
        path.write_bytes(path.read_bytes().replace(b'\r\n',b'\n'))
files=[{'path':p.relative_to(dest).as_posix(),'bytes':p.stat().st_size,'sha256':sha(p.read_bytes())} for p in sorted(dest.rglob('*')) if p.is_file() and p.name!='manifest.json']
(dest/'manifest.json').write_bytes((json.dumps({'schemaVersion':1,'visualAcceptance':'failed-user-review','scope':'Input diagnosis only; native static material frames and CPU skin reconstruction are separate from complete animated joint appearance.','files':files},indent=2)+'\n').encode('utf-8'));print(json.dumps({'files':len(files),'bytes':sum(p['bytes'] for p in files)}))
