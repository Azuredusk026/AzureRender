"""Preserve actual machine evidence and source bytes without claiming physical input."""
import gzip
import hashlib
import json
from pathlib import Path
import shutil
import subprocess

ROOT=Path.cwd().resolve();WORK=ROOT/'build/p4'
OUT=ROOT/'docs/acceptance/p4/evidence/2026-10-08';OUT.mkdir(parents=True,exist_ok=True)
records={}
def preserve(path,name,compressed=False):
    data=path.read_bytes();target=OUT/name;target.parent.mkdir(parents=True,exist_ok=True)
    target.write_bytes(gzip.compress(data,mtime=0) if compressed else data)
    records[name]=dict(sha256=hashlib.sha256(target.read_bytes()).hexdigest(),originalSha256=hashlib.sha256(data).hexdigest(),bytes=len(data))
def latest(pattern):return max(ROOT.glob(pattern),key=lambda p:p.stat().st_mtime)
for name in ('debug-full-red.log','debug-final-full.log','release-full.log','debug-final-build.log','release-final-build.log',
             'optimized-contracts.log','performance-red.log','workspace-input.log','workspace.log','workflow-supplement.log','docs-style.log','docs-site.log','docs-links.log','delivery.log','intel.log','performance.log'):
    path=WORK/name
    if path.exists():preserve(path,name)
for name in ('debug-detail-red.log','debug-final-detail.log','release-detail.log'):
    preserve(WORK/name,name+'.gz',True)
preserve(WORK/'test-discovery.json','test-discovery.json')
for config in ('debug','release'):
    build=ROOT/('build/ninja-msvc-'+config)
    preserve(build/'build-provenance.json',config+'-provenance.json')
    for category,subdir in [('native','p4/native'),('usability','u5/usability'),('documents','u5/documents'),('content','u7/content'),
                            ('selection','u6-selection'),('gizmo','u6-gizmo')]:
        folder=max((build/subdir).glob('run-*'),key=lambda p:p.stat().st_mtime)
        preserve(folder/'summary.json',config+'/'+category+'-summary.json')
        if category=='native':
            for workflow in ('exploration','inspector'):
                logs='\n'.join((folder/workflow/name).read_text(encoding='utf-8') for name in ('stdout.log','stderr.log'))
                assert 'Allocator after unload: buffers=0 images=0' in logs
                assert not any(s in logs for s in ('VUID-','Validation Error','SYNC-HAZARD'))
                runtime=json.loads((folder/workflow/'runtime.json').read_text(encoding='utf-8'))
                assert not runtime['scriptErrors'] and not runtime['presentationErrors']
        for path in folder.rglob('*'):
            if path.is_file() and path.name in ('events.json','control-results.json','input.json','ui-input.json','runtime.json','stdout.log','stderr.log'):
                preserve(path,config+'/'+category+'/'+path.relative_to(folder).as_posix()+'.gz',True)
workspace_input=latest('build/p4/workspace-input/run-*/summary.json').parent
preserve(workspace_input/'summary.json','workspace-input-summary.json')
for path in workspace_input.rglob('*'):
    if path.is_file() and path.name in ('ui-input.json','runtime.json','stdout.log','stderr.log'):
        preserve(path,'workspace-input/'+path.relative_to(workspace_input).as_posix()+'.gz',True)
workspace=latest('build/p4/workspace/run-*/summary.json').parent
preserve(workspace/'summary.json','workspace-summary.json')
for path in workspace.glob('*/*/capture/frame_000015.png'):
    preserve(path,'workspace/'+path.relative_to(workspace).as_posix())
for path in workspace.glob('*/*/workspace.json'):
    preserve(path,'workspace/'+path.relative_to(workspace).as_posix()+'.gz',True)
preserve(WORK/'workspace-contact.png','workspace-contact.png')
supplement=latest('build/p4/workflow-supplement/run-*/summary.json').parent
preserve(supplement/'summary.json','workflow-supplement-summary.json')
for path in supplement.glob('*/*/*'):
    if path.is_file() and path.name in ('ui-input.json','runtime.json','stdout.log','stderr.log'):
        preserve(path,'workflow-supplement/'+path.relative_to(supplement).as_posix()+'.gz',True)
for path in (WORK/'performance').glob('*'):
    if path.is_file():preserve(path,'performance/'+path.name+('.gz' if path.suffix=='.csv' or path.name not in ('summary.json','strict-budget.json') else ''),path.suffix=='.csv' or path.name not in ('summary.json','strict-budget.json'))
native_visual=latest('build/p4/native-framing/run-*/summary.json').parent
preserve(native_visual/'summary.json','native-framing-summary.json')
for workflow in ('exploration','inspector'):
    for path in (native_visual/workflow).glob('*'):
        if path.is_file() and path.name in ('summary.json','events.json','runtime.json','stdout.log','stderr.log','double-frame.png'):
            preserve(path,'native-framing/'+workflow+'/'+path.name+('' if path.suffix=='.png' else '.gz'),path.suffix!='.png')
preserve(WORK/'native-framing-final.log','native-framing-final.log')
for category in ('performance-red','profile'):
    for path in (WORK/category).glob('*'):
        if path.is_file():
            preserve(path,category+'/'+path.name+'.gz',True)
for category in ('delivery','intel'):
    folder=latest('build/p4/'+category+'/run-*/summary.json').parent
    preserve(folder/'summary.json',category+'-summary.json')
    for path in folder.rglob('*'):
        if path.is_file() and (path.suffix=='.log' or path.name in ('runtime.json','input.json','probe.json','gpu_capabilities.json')):
            preserve(path,category+'/'+path.relative_to(folder).as_posix()+'.gz',True)
for path in (ROOT/'CMakeLists.txt',ROOT/'tools/test_editor_native_input.py',ROOT/'src/editor/commands/EditService.cpp',ROOT/'src/editor/commands/EditService.hpp'):
    preserve(path,'source/'+path.name)
for path in WORK.glob('*.py'):
    if path.name not in ('framing_visual.py','framing_focus_visual.py'):preserve(path,'scripts/'+path.name)
# Build the twelve-task matrix from actual two-project summaries.
tasks=[
 ('右键观察及出界释放',[('native',['rightLook','outsideRelease']),('usability',['navigationDirections'])]),
 ('飞行、加速、停止与失焦',[('native',['flight','flightRelease','focusRelease']),('usability',['flight','focusRelease'])]),
 ('大纲单击、双击与定位',[('native',['singleOnlySelects','doubleFrames','fFrames'])]),
 ('工具切换及文本归属',[('native',['tools'])]),
 ('世界、局部与多选变换',[('gizmo',['commit','singleUndo'])]),
 ('取消及吸附',[('gizmo',['escape','snap','rotationSnap','scaleSnap'])]),
 ('搜索、多选及拾取',[('selection',['filteredCtrl','filteredRange','blankClear'])]),
 ('节点及层级制作',[('content',['reparent','rootDrop'])]),
 ('导入及射线放置',[('content',['closedImport','geometryPlacement'])]),
 ('属性、组件及错误恢复',[('content',['longName','longComponent','reset','remove','errorDismiss'])]),
 ('保存、关闭及重开',[('content',['saveReopen']),('documents',['passed'])]),
 ('运行预览及独立构建',[('usability',['previewCameraRestore','sceneBytes'])])]
matrix=[]
for configuration in ('debug','release'):
    for workflow in ('exploration','inspector'):
        for title,requirements in tasks:
            proof=[]
            for category,fields in requirements:
                name=configuration+'/'+category+'-summary.json'
                data=json.loads((OUT/name).read_text(encoding='utf-8'))
                rows=[data[workflow]] if isinstance(data,dict) else [r for r in data if r['workflow']==workflow]
                assert rows and all(row['passed'] and all(row[field] for field in fields) for row in rows),(title,configuration,workflow)
                proof.append(dict(file=name,fields=fields))
            matrix.append(dict(task=title,configuration=configuration,workflow=workflow,machinePassed=True,evidence=proof,physicalKeyboardMouse='NotExecuted'))
supplement=json.loads((OUT/'workflow-supplement-summary.json').read_text(encoding='utf-8'))
assert len(supplement)==2 and all(r['passed'] and r['createDuplicateDeleteUndo'] and r['worldLocal'] and r['multiParentChild'] and r['singleUndo'] for r in supplement)
workspace_data=json.loads((OUT/'workspace-input-summary.json').read_text(encoding='utf-8'))
assert workspace_data['passed'] and workspace_data['textFocusProtection'] and workspace_data['viewportPicking']
delivery_data=json.loads((OUT/'delivery-summary.json').read_text(encoding='utf-8'))
assert delivery_data['passed'] and len(delivery_data['packages'])==2
report=dict(schemaVersion=1,rows=matrix,sharedChecks=['workflow-supplement-summary.json','workspace-input-summary.json','delivery-summary.json'],physicalKeyboardMouse='NotExecuted')
(WORK/'tasks.json').write_text(json.dumps(report,ensure_ascii=False,indent=2),encoding='utf-8')
preserve(WORK/'tasks.json','tasks.json')
manifest=dict(schemaVersion=1,phase='P4',status='MachineVerifiedPhysicalInputPending',baseCommit=subprocess.check_output(['git','rev-parse','HEAD'],text=True).strip(),
    debug=dict(passed=168,cpu=123,gpu=45),release=dict(passed=170,cpu=123,gpu=47),
    initialDebug=dict(passed=167,failed=1,failedTest='AzureEditor.NativeInputGpu',reason='Synthetic double-click interval exceeded 300 ms'),
    physicalKeyboardMouse='NotExecuted',workspaceCases=12,supplementProjects=2,files=records)
assert '100% tests passed, 0 tests failed out of 168' in (WORK/'debug-final-full.log').read_text()
assert '100% tests passed, 0 tests failed out of 170' in (WORK/'release-full.log').read_text()
assert json.loads((WORK/'performance/strict-budget.json').read_text())['passed']
(OUT/'verification.json').write_text(json.dumps(manifest,indent=2),encoding='utf-8')
print(json.dumps(dict(status=manifest['status'],files=len(records))),flush=True)
