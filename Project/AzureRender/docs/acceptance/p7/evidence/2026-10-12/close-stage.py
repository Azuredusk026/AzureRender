from pathlib import Path
import difflib,gzip,hashlib,json,shutil,subprocess,sys
root=Path.cwd().resolve();out=root/'build/p7-first-batch-delivery-2026-10-12';dest=root/'docs/acceptance/p7/evidence/2026-10-12'
for c in ('debug','release'):
    assert '100% tests passed, 0 tests failed out of 172' in (out/(c+'-cpu.log')).read_text(encoding='utf-8'),c
    assert '100% tests passed, 0 tests failed out of 7' in (out/(c+'-gpu.log')).read_text(encoding='utf-8'),c
sys.path.insert(0,str(root/'tools'));from build_provenance import verify
proofs={c:json.loads((root/('build/ninja-msvc-'+c.lower())/'build-provenance.json').read_text(encoding='utf-8')) for c in ('Debug','Release')}
for proof in proofs.values():verify(root,proof)
assert proofs['Debug']['source']==proofs['Release']['source']
delivery=json.loads((out/'delivery-summary.json').read_text(encoding='utf-8'));assert delivery['passed'] and len(delivery['rows'])==4
baseline=json.loads((out/'stage-baseline.json').read_text(encoding='utf-8'))
def save(p,s):
    temporary=p.with_suffix(p.suffix+'.p7tmp');temporary.write_text(s,encoding='utf-8');temporary.replace(p)
def shared(name,transform):
    entry=baseline[name];assert (root/name).read_text(encoding='utf-8')==entry['work'],name
    save(root/name,transform(entry['work']))
    blob=subprocess.check_output(['git','hash-object','-w','--stdin'],input=transform(entry['head']).encode()).decode().strip()
    subprocess.run(['git','update-index','--cacheinfo','100644,'+blob+',Project/AzureRender/'+name],check=True)
readme="""P7 第一批格式制作与交付通过自动验收。
两个空文档项目完成三类资产制作及往返。
材料、声音和动画通过实际生产路径验证。
两配置各一百七十二项 CPU 回归通过。

证据见[P7 验收](docs/acceptance/p7/2026-10-12.md)。
当前连续执行 G22 压缩纹理与 mip 交换。
正式性能及人工复核集中在 P8。

"""
shared('README.md',lambda s:s.replace('# AzureRender 工程\n\n','# AzureRender 工程\n\n'+readme,1).replace('当前连续执行 P7 第一批格式交付验收。','P7 结果按所属阶段来源维护。'))
record="""## P7 第一批格式交付 2026-10-12

新增两个空文档项目的公共制作与交换验收。
三类资产、材料槽及导入设置通过保存重开。
导入完整性、失败恢复与实际动画采样通过。
两个独立包完成双设备移动及隔离运行。

证据见[P7 验收](docs/acceptance/p7/2026-10-12.md)。
正式性能、长跑及人工复核集中在 P8。

"""
shared('CHANGELOG.md',lambda s:s.replace('# Changelog\n\n','# Changelog\n\n'+record,1))
plan="""## P7 自动验收结果

第一批格式制作与交付的自动门禁通过。
两配置各一百七十二项 CPU 回归通过。
两配置各七项受影响 GPU 回归通过。
两个项目完成双设备移动包与实际播放检查。

证据见[P7 验收](../acceptance/p7/2026-10-12.md)。
当前连续执行 G22 压缩纹理与 mip 交换。
正式性能、长跑及集中人工复核在 P8 验收。

"""
shared('docs/plans/azure-engine-plan.md',lambda s:s.replace('# Azure Engine 开发总计划\n\n','# Azure Engine 开发总计划\n\n'+plan,1).replace('当前连续执行 P7 第一批格式交付验收。','P7 结果按所属阶段来源维护。'))
p=root/'docs/plans/asset-format-manifest.json';d=json.loads(p.read_text(encoding='utf-8'))
for phase in d['phases']:
    if phase['id']=='P7':phase.update(status='AutomaticVerifiedFinalAcceptanceDeferred',automaticComplete=True,evidence='docs/acceptance/p7/evidence/2026-10-12/manifest.json')
    if phase['id']=='G22':phase['status']='Active'
d['evidence']['firstBatchDelivery']='docs/acceptance/p7/evidence/2026-10-12/manifest.json';save(p,json.dumps(d,ensure_ascii=False,indent=2)+'\n')
p=root/'docs/plans/hazel-fermion-manifest.json';d=json.loads(p.read_text(encoding='utf-8'));d['closureProgress']['currentAutomaticPhase']='G22';save(p,json.dumps(d,ensure_ascii=False,indent=2)+'\n')
p=root/'docs/plans/2026-10-11-next-development.md';save(p,p.read_text(encoding='utf-8').replace('U15 至 U17 的已实施阶段自动门禁通过。','U15 至 P7 的已实施阶段自动门禁通过。').replace('当前执行 P7 第一批格式制作与交付验收。','当前执行 G22 压缩纹理与 mip 交换。').replace('证据见[U17 验收](../acceptance/u17/2026-10-12.md)。','证据见[P7 验收](../acceptance/p7/2026-10-12.md)。',1))
p=root/'docs/plans/2026-10-11-asset-format-expansion.md';s=p.read_text(encoding='utf-8').replace('状态：U17 自动门禁通过，P7 执行中','状态：P7 自动门禁通过，G22 执行中');a=s.index('## P7：');b=s.index('## G22：',a);section=s[a:b].replace('- [ ]','- [x]').replace('- [x] 在 P8 按当前严格预算执行正式性能与长跑。','- [ ] 在 P8 按当前严格预算执行正式性能与长跑。');save(p,s[:a]+section+s[b:])
p=root/'docs/plans/2026-10-12-first-batch-delivery.md';save(p,p.read_text(encoding='utf-8').replace('状态：执行中。','状态：非延期自动门禁通过。').replace('- [ ]','- [x]'))
p=root/'docs/acceptance/p7/2026-10-12.md';save(p,p.read_text(encoding='utf-8').replace('状态：完整回归与独立交付验证执行中。','状态：非延期自动门禁通过。').replace('原字节证据在全部自动门禁通过后归档。','证据见[原字节清单](evidence/2026-10-12/manifest.json)。')+"""
## 自动结果

Debug 与 Release 各一百七十二项 CPU 回归通过。
两配置各七项受影响 GPU 回归通过。
两个项目的四组设备组合均通过隔离运行。
实际材料、声音和动画帧通过验证。

文风及严格站点构建检查通过。
全库既有标点诊断按 U17 证据在 P8 复核。
正式性能、长跑及实体听感保持待验状态。
""")
# Refresh installed documents and their byte inventory after synchronization.
for name,args in [('install-final',['cmake','--install',root/'build/ninja-msvc-release','--config','Release','--prefix',out/'Engine unavailable']),('install-final-manifest',['cmake','-DINSTALL_DIR='+(out/'Engine unavailable').as_posix(),'-DOUTPUT_FILE='+str(out/'install-manifest.json'),'-P',root/'tools/write_install_manifest.cmake']),('install-final-verify',['cmake','-DINSTALL_DIR='+(out/'Engine unavailable').as_posix(),'-DMANIFEST_FILE='+str(out/'install-manifest.json'),'-P',root/'tools/verify_install_manifest.cmake'])]:
    r=subprocess.run(list(map(str,args)),capture_output=True,encoding='utf-8',errors='replace',timeout=300);(out/(name+'.log')).write_bytes((r.stdout+r.stderr).encode());assert r.returncode==0,r.stdout+r.stderr
delivery['fileCount']=json.loads((out/'install-manifest.json').read_text(encoding='utf-8'))['file_count'];delivery['installedDocumentsMatch']=True
for relative in ('runtime/asset-exchange.md','plans/asset-format-manifest.json','plans/azure-engine-plan.md'):assert (out/'Engine unavailable/share/AzureRender/docs'/relative).read_bytes()==(root/'docs'/relative).read_bytes()
(out/'delivery-summary.json').write_text(json.dumps(delivery,indent=2)+'\n',encoding='utf-8')
dest.mkdir(parents=True,exist_ok=True)
for file in out.glob('*.log'):(dest/(file.name+'.gz')).write_bytes(gzip.compress(file.read_bytes(),mtime=0))
for file in out.glob('*.py'):shutil.copy2(file,dest/file.name)
for c,proof in proofs.items():(dest/(c.lower()+'-provenance.json.gz')).write_bytes(gzip.compress(json.dumps(proof,indent=2).encode(),mtime=0))
for name in ('delivery-summary.json',):shutil.copy2(out/name,dest/name)
(dest/'authoring-summary.json').write_bytes((out/'Sources unavailable/authoring-summary.json').read_bytes())
(dest/'install-manifest.json.gz').write_bytes(gzip.compress((out/'install-manifest.json').read_bytes(),mtime=0))
for device in ('nvidia','intel'):
    for workflow in ('workshop','assembly'):
        name=device+'-'+workflow;shutil.copy2(out/(name+'-runtime.json'),dest/(name+'-runtime.json'))
        for file in (out/(name+'-capture')).glob('*'):
            if file.is_file():shutil.copy2(file,dest/(name+'-'+file.name))
for workflow in ('workshop','assembly'):(dest/(workflow+'-package-manifest.json.gz')).write_bytes(gzip.compress((out/('Moved '+workflow)/'game_manifest.json').read_bytes(),mtime=0))
(dest.parent/'.gitattributes').write_text('** -text -whitespace\n',encoding='utf-8')
manifest={'schemaVersion':1,'phase':'P7','automaticComplete':True,'finalAcceptancePhase':'P8','sourceSnapshotSha256':hashlib.sha256(json.dumps(proofs['Release']['source'],sort_keys=True).encode()).hexdigest(),'cpu':{'Debug':172,'Release':172},'gpuRegression':{'Debug':7,'Release':7},'emptyProjects':2,'independentPackages':2,'deviceWorkflowCombinations':4,'manualAcceptance':'DeferredToP8','installedFileCount':delivery['fileCount'],'firstBatchTestSha256':hashlib.sha256((root/'tests/FirstBatchAuthoringTests.cpp').read_bytes()).hexdigest(),'firstBatchTestBinaries':{c:hashlib.sha256((root/('build/ninja-msvc-'+c)/'AzureFirstBatchAuthoringTests.exe').read_bytes()).hexdigest() for c in ('debug','release')},'files':[]}
for p in sorted(dest.rglob('*')):
    if p.is_file() and p!=dest/'manifest.json':manifest['files'].append({'path':p.relative_to(dest).as_posix(),'sha256':hashlib.sha256(p.read_bytes()).hexdigest(),'bytes':p.stat().st_size})
(dest/'manifest.json').write_text(json.dumps(manifest,indent=2)+'\n',encoding='utf-8')
# Shared documents have separate HEAD and WORK transformations.
for name in ('CMakeLists.txt',):
    path='Project/AzureRender/'+name;patch=''.join(difflib.unified_diff(baseline[name]['work'].splitlines(keepends=True),(root/name).read_text(encoding='utf-8').splitlines(keepends=True),fromfile='a/'+path,tofile='b/'+path));subprocess.run(['git','apply','--cached','--check','-'],input=patch.encode(),check=True);subprocess.run(['git','apply','--cached','-'],input=patch.encode(),check=True)
subprocess.run(['git','add','--','tests/FirstBatchAuthoringTests.cpp','docs/plans/2026-10-12-first-batch-delivery.md','docs/acceptance/p7/2026-10-12.md','docs/plans/asset-format-manifest.json','docs/plans/hazel-fermion-manifest.json','docs/plans/2026-10-11-next-development.md','docs/plans/2026-10-11-asset-format-expansion.md'],check=True)
subprocess.run(['git','add','-f','--',str(dest.parent)],check=True)
for row in manifest['files']:
    blob=subprocess.check_output(['git','show',':Project/AzureRender/'+dest.relative_to(root).as_posix()+'/'+row['path']]);assert hashlib.sha256(blob).hexdigest()==row['sha256'],row['path']
print('P7 exact stage and',len(manifest['files']),'raw blobs verified; source',manifest['sourceSnapshotSha256'])
