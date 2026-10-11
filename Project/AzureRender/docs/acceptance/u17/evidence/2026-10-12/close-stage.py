from pathlib import Path
import difflib,gzip,hashlib,json,shutil,subprocess,sys
root=Path.cwd();out=root/'build/u17-asset-transfer-2026-10-12';dest=root/'docs/acceptance/u17/evidence/2026-10-12'
for config in ('debug','release'):
    assert '100% tests passed, 0 tests failed out of 171' in (out/(config+'-cpu.log')).read_text(encoding='utf-8'),config
    assert '100% tests passed, 0 tests failed out of 7' in (out/(config+'-gpu.log')).read_text(encoding='utf-8'),config
sys.path.insert(0,str(root/'tools'));from build_provenance import verify
proofs={c:json.loads((root/('build/ninja-msvc-'+c.lower())/'build-provenance.json').read_text(encoding='utf-8')) for c in ('Debug','Release')}
for proof in proofs.values():verify(root,proof)
assert proofs['Debug']['source']==proofs['Release']['source']
delivery=json.loads((out/'delivery-summary.json').read_text(encoding='utf-8'));assert delivery['passed']
baseline=json.loads((out/'stage-baseline.json').read_text(encoding='utf-8'))
def save(p,s):
    temp=p.with_suffix(p.suffix+'.u17tmp');temp.write_text(s,encoding='utf-8');temp.replace(p)
def shared(name,transform):
    entry=baseline[name];assert (root/name).read_text(encoding='utf-8')==entry['work'],name
    save(root/name,transform(entry['work']))
    blob=subprocess.check_output(['git','hash-object','-w','--stdin'],input=transform(entry['head']).encode()).decode().strip()
    subprocess.run(['git','update-index','--cacheinfo','100644,'+blob+',Project/AzureRender/'+name],check=True)
readme="""U17 资产交换体验通过非延期自动门禁。
资产及子树导出共用规划与任务服务。
中文及表情字符路径通过双宿主验证。
两配置各一百七十一项 CPU 回归通过。

证据见[U17 验收](docs/acceptance/u17/2026-10-12.md)。
当前连续执行 P7 第一批格式交付验收。
实体复核及正式性能集中在 P8。

"""
shared('README.md',lambda s:s.replace('# AzureRender 工程\n\n','# AzureRender 工程\n\n'+readme,1).replace('当前连续执行 U17 资产交换体验阶段。','U17 结果按所属阶段来源维护。'))
record="""## U17 资产交换体验 2026-10-12

新增资产及子树的公共导出操作和界面。
格式设置由注册描述生成并经领域服务校验。
规划过期、取消、覆盖重试及文档保护通过。
修复两宿主 Unicode 输出及补充字符输入。

证据见[U17 验收](docs/acceptance/u17/2026-10-12.md)。
实体操作和正式性能在 P8 集中验收。

"""
shared('CHANGELOG.md',lambda s:s.replace('# Changelog\n\n','# Changelog\n\n'+record,1))
plan="""## U17 自动验收结果

U17 资产交换体验的非延期自动门禁通过。
两配置各一百七十一项 CPU 回归通过。
两配置各七项受影响 GPU 回归通过。
移动安装及双设备交换工作流通过。

证据见[U17 验收](../acceptance/u17/2026-10-12.md)。
当前连续执行 P7 第一批格式交付验收。
正式性能、长跑及集中人工复核在 P8 验收。

"""
shared('docs/plans/azure-engine-plan.md',lambda s:s.replace('# Azure Engine 开发总计划\n\n','# Azure Engine 开发总计划\n\n'+plan,1).replace('当前连续执行 U17 资产交换体验阶段。','U17 结果按所属阶段来源维护。'))
p=root/'docs/plans/asset-format-manifest.json';data=json.loads(p.read_text(encoding='utf-8'))
for phase in data['phases']:
    if phase['id']=='U17':
        phase.update(status='AutomaticVerifiedFinalAcceptanceDeferred',automaticComplete=True,evidence='docs/acceptance/u17/evidence/2026-10-12/manifest.json')
        for issue in phase.get('followUpIssues',[]):issue.update(status='AutomaticVerified',resolutionEvidence=phase['evidence'])
    if phase['id']=='P7':phase['status']='Active'
save(p,json.dumps(data,ensure_ascii=False,indent=2)+'\n')
p=root/'docs/plans/hazel-fermion-manifest.json';data=json.loads(p.read_text(encoding='utf-8'));data['closureProgress']['currentAutomaticPhase']='P7';save(p,json.dumps(data,ensure_ascii=False,indent=2)+'\n')
p=root/'docs/plans/2026-10-11-asset-format-expansion.md';s=p.read_text(encoding='utf-8').replace('状态：G21 自动门禁通过，U17 执行中','状态：U17 自动门禁通过，P7 执行中');a=s.index('## U17：');b=s.index('## P7：',a);section=s[a:b].replace('- [ ]','- [x]').replace('- [x] 操作者复核路径、反馈与恢复理解性。','- [ ] 在 P8 由操作者复核路径、反馈与恢复理解性。');save(p,s[:a]+section+s[b:])
p=root/'docs/plans/2026-10-11-next-development.md';save(p,p.read_text(encoding='utf-8').replace('U15 至 G21 的已实施阶段自动门禁通过。','U15 至 U17 的已实施阶段自动门禁通过。').replace('当前执行 U17 导入导出与重导入体验。','当前执行 P7 第一批格式制作与交付验收。').replace('证据见[G21 验收](../acceptance/g21/2026-10-12.md)。','证据见[U17 验收](../acceptance/u17/2026-10-12.md)。',1))
p=root/'docs/plans/2026-10-12-asset-transfer-experience.md';save(p,p.read_text(encoding='utf-8').replace('状态：执行中。','状态：非延期自动门禁通过。').replace('- [ ]','- [x]'))
p=root/'docs/acceptance/u17/2026-10-12.md';save(p,p.read_text(encoding='utf-8').replace('状态：自动回归与交付验证执行中。','状态：非延期自动门禁通过。')+"""
## 自动结果

Debug 与 Release 各一百七十一项 CPU 回归通过。
两配置各七项受影响 GPU 回归通过。
移动安装的两个工作流完成双设备回放。
两个宿主的 Unicode 出口完成双设备验证。

来源及日志见[原字节清单](evidence/2026-10-12/manifest.json)。
安装清单、许可及独立交换工具核验通过。
人工操作及正式性能保持 P8 待验状态。
""")
dest.mkdir(parents=True,exist_ok=True)
for file in out.glob('*.log'):(dest/(file.name+'.gz')).write_bytes(gzip.compress(file.read_bytes(),mtime=0))
for name in ('install-manifest.json',):(dest/(name+'.gz')).write_bytes(gzip.compress((out/name).read_bytes(),mtime=0))
for name in ('delivery-summary.json','module-boundaries.json','module-boundaries-release.json'):shutil.copy2(out/name,dest/name)
for config,proof in proofs.items():(dest/(config.lower()+'-provenance.json.gz')).write_bytes(gzip.compress(json.dumps(proof,indent=2).encode(),mtime=0))
for folder in ('ui-wchar32-green','ui-nvidia','ui-intel'):
    for file in (out/folder).rglob('runtime.json'):
        target=dest/folder/file.relative_to(out/folder);target.parent.mkdir(parents=True,exist_ok=True);shutil.copy2(file,target)
    shutil.copy2(out/folder/'summary.json',dest/(folder+'-summary.json'))
    for workflow in ('asset-export','subtree-export'):
        file=sorted((out/folder/workflow/'capture').glob('frame_*.png'))[-1];shutil.copy2(file,dest/(folder+'-'+workflow+'.png'))
for name in ('nvidia','intel'):
    for file in (out/(name+'-capture')).glob('*'):
        if file.is_file():shutil.copy2(file,dest/(name+'-'+file.name))
    shutil.copy2(out/(name+'-runtime.json'),dest/(name+'-runtime.json'))
(dest/'package-manifest.json.gz').write_bytes(gzip.compress((out/'Moved exchange game/game_manifest.json').read_bytes(),mtime=0))
preservation={'schemaVersion':1,'preexistingEditsExcludedFromStaging':True,'files':[]}
for name,entry in baseline.items():
    current=(root/name).read_text(encoding='utf-8')
    preservation['files'].append({'path':name,'baselineHeadSha256':hashlib.sha256(entry['head'].encode()).hexdigest(),'baselineWorkSha256':hashlib.sha256(entry['work'].encode()).hexdigest(),'currentWorkSha256':hashlib.sha256(current.encode()).hexdigest(),'authorizedIncrement':current!=entry['work'],'preexistingEdits':entry['head']!=entry['work']})
(dest/'user-source-preservation.json').write_text(json.dumps(preservation,indent=2)+'\n',encoding='utf-8')
for file in out.glob('*.py'):shutil.copy2(file,dest/file.name)
(dest.parent/'.gitattributes').write_text('** -text -whitespace\n',encoding='utf-8')
manifest={'schemaVersion':1,'phase':'U17','automaticComplete':True,'finalAcceptancePhase':'P8','sourceSnapshotSha256':hashlib.sha256(json.dumps(proofs['Release']['source'],sort_keys=True).encode()).hexdigest(),'sourceScope':'Complete working-tree snapshot preserving existing user edits','cpu':{'Debug':171,'Release':171},'gpuRegression':{'Debug':7,'Release':7},'productionUiWorkflows':2,'devices':['NVIDIA','Intel'],'installedFileCount':delivery['fileCount'],'packageFileCount':delivery['packageFiles'],'manualAcceptance':'DeferredToP8','files':[]}
for file in sorted(dest.rglob('*')):
    if file.is_file() and file.name!='manifest.json':manifest['files'].append({'path':file.relative_to(dest).as_posix(),'sha256':hashlib.sha256(file.read_bytes()).hexdigest(),'bytes':file.stat().st_size})
(dest/'manifest.json').write_text(json.dumps(manifest,indent=2)+'\n',encoding='utf-8')
# Exact authorized increments, with preexisting changes retained in the working tree.
for name,entry in baseline.items():
    if name in ('README.md','CHANGELOG.md','docs/plans/azure-engine-plan.md'):continue
    new=(root/name).read_text(encoding='utf-8');path='Project/AzureRender/'+name
    patch=''.join(difflib.unified_diff(entry['work'].splitlines(keepends=True),new.splitlines(keepends=True),fromfile='a/'+path,tofile='b/'+path))
    if patch:
        subprocess.run(['git','apply','--cached','--check','-'],input=patch.encode(),check=True)
        subprocess.run(['git','apply','--cached','-'],input=patch.encode(),check=True)
paths=['src/editor/content/AssetExportService.hpp','src/editor/content/AssetExportService.cpp','src/editor/panels/AssetTransferPanel.cpp','tests/EditorAssetTransferTests.cpp','tools/test_asset_transfer_workflows.py','tools/test_unicode_outputs.py','docs/runtime/asset-exchange.md','docs/plans/2026-10-12-asset-transfer-experience.md','docs/plans/asset-format-manifest.json','docs/plans/hazel-fermion-manifest.json','docs/plans/2026-10-11-asset-format-expansion.md','docs/plans/2026-10-11-next-development.md','docs/acceptance/u17/2026-10-12.md']
subprocess.run(['git','add','--',*paths],check=True);subprocess.run(['git','add','-f','--',str(dest.parent)],check=True)
for entry in manifest['files']:
    blob=subprocess.check_output(['git','show',':Project/AzureRender/'+dest.relative_to(root).as_posix()+'/'+entry['path']]);assert hashlib.sha256(blob).hexdigest()==entry['sha256'],entry['path']
print('Verified',len(manifest['files']),'raw evidence blobs; precise U17 staging; source',manifest['sourceSnapshotSha256'])
