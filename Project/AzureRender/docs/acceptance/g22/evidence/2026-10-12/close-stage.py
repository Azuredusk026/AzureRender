from pathlib import Path
import difflib,gzip,hashlib,json,shutil,subprocess,sys
root=Path.cwd().resolve();out=root/'build/g22-texture-containers-2026-10-12';dest=root/'docs/acceptance/g22/evidence/2026-10-12'
for c in ('debug','release'):
    assert '100% tests passed, 0 tests failed out of 173' in (out/(c+'-budget-final-cpu.log')).read_text(encoding='utf-8'),c
    assert '100% tests passed, 0 tests failed out of 8' in (out/(c+'-budget-final-gpu.log')).read_text(encoding='utf-8'),c
sys.path.insert(0,str(root/'tools'));from build_provenance import verify
proofs={c:json.loads((root/('build/ninja-msvc-'+c.lower())/'build-provenance.json').read_text(encoding='utf-8')) for c in ('Debug','Release')}
for proof in proofs.values():verify(root,proof)
assert proofs['Debug']['source']==proofs['Release']['source']
delivery=json.loads((out/'delivery-summary.json').read_text(encoding='utf-8'));assert delivery['passed'] and len(delivery['rows'])==4
baseline=json.loads((out/'stage-baseline.json').read_text(encoding='utf-8'))
def save(p,s):
    temporary=p.with_suffix(p.suffix+'.g22tmp');temporary.write_text(s,encoding='utf-8');temporary.replace(p)
def shared(name,transform):
    entry=baseline[name];assert (root/name).read_text(encoding='utf-8') in (entry['work'],transform(entry['work'])),name
    save(root/name,transform(entry['work']))
    blob=subprocess.check_output(['git','hash-object','-w','--stdin'],input=transform(entry['head']).encode()).decode().strip()
    subprocess.run(['git','update-index','--cacheinfo','100644,'+blob+',Project/AzureRender/'+name],check=True)
readme="""G22 二维 DDS/KTX2 交换通过自动验收。
原始 mip 进入公共材料与实际纹理上传。
两个项目完成双设备移动包验证。

证据见[G22 验收](docs/acceptance/g22/2026-10-12.md)。
当前连续执行 G23 压缩音频与容器交换。
正式性能及人工复核集中在 P8。

"""
shared('README.md',lambda s:s.replace('# AzureRender 工程\n\n','# AzureRender 工程\n\n'+readme,1).replace('当前连续执行 G22 压缩纹理与 mip 交换。','G22 结果按所属阶段来源维护。'))
record="""## G22 二维纹理容器交换 2026-10-12

新增公共 DDS/KTX2 导入、导出与压缩设置。
原始 mip 经材料、glTF 及实际上传路径保留。
BC 与 Basis 误差、取消及非法输入通过验证。
两个项目完成双设备移动及隔离运行。

证据见[G22 验收](docs/acceptance/g22/2026-10-12.md)。
正式性能、长跑及人工复核集中在 P8。

"""
shared('CHANGELOG.md',lambda s:s.replace('# Changelog\n\n','# Changelog\n\n'+record,1))
plan="""## G22 自动验收结果

二维压缩纹理交换的自动门禁通过。
两配置各一百七十三项 CPU 回归通过。
两配置各八项受影响 GPU 回归通过。
两个项目完成双设备移动包与实际播放检查。

证据见[G22 验收](../acceptance/g22/2026-10-12.md)。
当前连续执行 G23 压缩音频与容器交换。
正式性能、长跑及集中人工复核在 P8 验收。

"""
shared('docs/plans/azure-engine-plan.md',lambda s:s.replace('# Azure Engine 开发总计划\n\n','# Azure Engine 开发总计划\n\n'+plan,1).replace('当前连续执行 G22 压缩纹理与 mip 交换。','G22 结果按所属阶段来源维护。'))
p=root/'docs/plans/asset-format-manifest.json';d=json.loads(p.read_text(encoding='utf-8'))
for phase in d['phases']:
    if phase['id']=='G22':phase.update(status='AutomaticVerifiedFinalAcceptanceDeferred',automaticComplete=True,evidence='docs/acceptance/g22/evidence/2026-10-12/manifest.json')
    if phase['id']=='G23':phase['status']='Active'
d['evidence']['textureContainers']='docs/acceptance/g22/evidence/2026-10-12/manifest.json'
for name in ('dds','ktx2'):
    d['verifiedCapabilities'].append({'id':name,'domain':'image','read':True,'write':True,'transformsSource':False,'settingsVersion':1,'generatorVersion':1,'evidence':'docs/acceptance/g22/evidence/2026-10-12/manifest.json','runtimePath':'BudgetedRgbaMipChain','textureDimensions':'Single2D','compression':(['rgba','bc1','bc3','bc5','bc7'] if name=='dds' else ['rgba','basis-uastc'])})
save(p,json.dumps(d,ensure_ascii=False,indent=2)+'\n')
p=root/'docs/plans/hazel-fermion-manifest.json';d=json.loads(p.read_text(encoding='utf-8'));d['closureProgress']['currentAutomaticPhase']='G23';save(p,json.dumps(d,ensure_ascii=False,indent=2)+'\n')
p=root/'docs/plans/2026-10-11-next-development.md';save(p,p.read_text(encoding='utf-8').replace('U15 至 P7 的已实施阶段自动门禁通过。','U15 至 G22 的已实施阶段自动门禁通过。').replace('当前执行 G22 压缩纹理与 mip 交换。','当前执行 G23 压缩音频与容器交换。').replace('证据见[P7 验收](../acceptance/p7/2026-10-12.md)。','证据见[G22 验收](../acceptance/g22/2026-10-12.md)。',1))
p=root/'docs/plans/2026-10-11-asset-format-expansion.md';s=p.read_text(encoding='utf-8').replace('状态：P7 自动门禁通过，G22 执行中','状态：G22 自动门禁通过，G23 执行中');a=s.index('## G22：');b=s.index('## G23：',a);save(p,s[:a]+s[a:b].replace('- [ ]','- [x]')+s[b:])
p=root/'docs/plans/2026-10-12-texture-containers.md';save(p,p.read_text(encoding='utf-8').replace('状态：执行中。','状态：非延期自动门禁通过。').replace('- [ ]','- [x]'))
p=root/'docs/acceptance/g22/2026-10-12.md';save(p,p.read_text(encoding='utf-8').replace('状态：最终回归与隔离交付执行中。','状态：非延期自动门禁通过。').replace('最终测试、来源与截图在门禁通过后归档。','证据见[原字节清单](evidence/2026-10-12/manifest.json)。')+"""

## 自动结果

Debug 与 Release 各一百七十三项 CPU 回归通过。
两配置各八项受影响 GPU 回归通过。
两个项目的四组设备组合均通过隔离运行。
设备验证包含材料、实际纹理与完整资源释放。

文风及严格站点构建检查通过。
全库既有标点诊断在 P8 复核。
正式性能、长跑及实体复核保持待验状态。
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
for name in ('etc1s-proof.json','contract-clean-recovery.json'):shutil.copy2(out/name,dest/name)
for file in (out/'etc1s-output').glob('*.png'):shutil.copy2(file,dest/file.name)
for folder in ('ui-final',):
    for file in (out/folder).rglob('*'):
        if file.is_file() and (file.suffix in ('.json','.log') or file.name=='frame_000099.png'):
            target=dest/folder/file.relative_to(out/folder);target.parent.mkdir(parents=True,exist_ok=True);shutil.copy2(file,target)
for c,proof in proofs.items():(dest/(c.lower()+'-provenance.json.gz')).write_bytes(gzip.compress(json.dumps(proof,indent=2).encode(),mtime=0))
for name in ('delivery-summary.json',):shutil.copy2(out/name,dest/name)
(dest/'authoring-summary.json').write_bytes((out/'Sources unavailable/authoring-summary.json').read_bytes())
(dest/'install-manifest.json.gz').write_bytes(gzip.compress((out/'install-manifest.json').read_bytes(),mtime=0))
for device in ('nvidia','intel'):
    for workflow in ('dds','basis'):
        name=device+'-'+workflow;shutil.copy2(out/(name+'-runtime.json'),dest/(name+'-runtime.json'))
        for file in (out/(name+'-capture')).glob('*'):
            if file.is_file():shutil.copy2(file,dest/(name+'-'+file.name))
for workflow in ('dds','basis'):(dest/(workflow+'-package-manifest.json.gz')).write_bytes(gzip.compress((out/('Moved '+workflow)/'game_manifest.json').read_bytes(),mtime=0))
(dest.parent/'.gitattributes').write_text('** -text -whitespace\n',encoding='utf-8')
manifest={'schemaVersion':1,'phase':'G22','automaticComplete':True,'finalAcceptancePhase':'P8','sourceSnapshotSha256':hashlib.sha256(json.dumps(proofs['Release']['source'],sort_keys=True).encode()).hexdigest(),'cpu':{'Debug':173,'Release':173},'gpuRegression':{'Debug':8,'Release':8},'emptyProjects':2,'independentPackages':2,'deviceWorkflowCombinations':4,'manualAcceptance':'DeferredToP8','installedFileCount':delivery['fileCount'],'textureContainerTestSha256':hashlib.sha256((root/'tests/TextureContainerTests.cpp').read_bytes()).hexdigest(),'textureContainerTestBinaries':{c:hashlib.sha256((root/('build/ninja-msvc-'+c)/'AzureTextureContainerTests.exe').read_bytes()).hexdigest() for c in ('debug','release')},'files':[]}
for p in sorted(dest.rglob('*')):
    if p.is_file() and p!=dest/'manifest.json':manifest['files'].append({'path':p.relative_to(dest).as_posix(),'sha256':hashlib.sha256(p.read_bytes()).hexdigest(),'bytes':p.stat().st_size})
(dest/'manifest.json').write_text(json.dumps(manifest,indent=2)+'\n',encoding='utf-8')
# Shared documents have separate HEAD and WORK transformations.
for name in ('CMakeLists.txt',):
    path='Project/AzureRender/'+name;patch=''.join(difflib.unified_diff(baseline[name]['work'].splitlines(keepends=True),(root/name).read_text(encoding='utf-8').splitlines(keepends=True),fromfile='a/'+path,tofile='b/'+path));subprocess.run(['git','apply','--cached','--check','-'],input=patch.encode(),check=True);subprocess.run(['git','apply','--cached','-'],input=patch.encode(),check=True)
subprocess.run(['git','add','--','tests/TextureContainerTests.cpp','tests/EditorAssetWorkflowTests.cpp','docs/plans/2026-10-12-texture-containers.md','docs/acceptance/g22/2026-10-12.md','docs/plans/asset-format-manifest.json','docs/plans/hazel-fermion-manifest.json','docs/plans/2026-10-11-next-development.md','docs/plans/2026-10-11-asset-format-expansion.md'],check=True)
subprocess.run(['git','add','-f','--',str(dest.parent)],check=True)
for row in manifest['files']:
    blob=subprocess.check_output(['git','show',':Project/AzureRender/'+dest.relative_to(root).as_posix()+'/'+row['path']]);assert hashlib.sha256(blob).hexdigest()==row['sha256'],row['path']
print('G22 exact stage and',len(manifest['files']),'raw blobs verified; source',manifest['sourceSnapshotSha256'])
