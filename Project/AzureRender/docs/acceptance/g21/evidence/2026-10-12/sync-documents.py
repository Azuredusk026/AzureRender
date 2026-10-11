from pathlib import Path
import json,subprocess
root=Path.cwd();out=root/'build/g21-dependency-evaluation-2026-10-12'
for config in ('debug','release'):
    assert '100% tests passed, 0 tests failed out of 170' in (out/(config+'-cpu.log')).read_text(encoding='utf-8')
    assert '100% tests passed, 0 tests failed out of 4' in (out/(config+'-gpu.log')).read_text(encoding='utf-8')
assert json.loads((out/'delivery-summary.json').read_text(encoding='utf-8'))['passed']
baseline=json.loads((out/'stage-baseline.json').read_text(encoding='utf-8'))
def save(path,value):
    temp=path.with_suffix(path.suffix+'.g21tmp');temp.write_text(value,encoding='utf-8');temp.replace(path)
def shared(name,transform):
    entry=baseline[name];assert (root/name).read_text(encoding='utf-8')==entry['work'],name
    save(root/name,transform(entry['work']))
    blob=subprocess.check_output(['git','hash-object','-w','--stdin'],input=transform(entry['head']).encode()).decode().strip()
    subprocess.run(['git','update-index','--cacheinfo','100644,'+blob+',Project/AzureRender/'+name],check=True)
readme="""G21 通用导出与往返通过自动验收。
十六种输出共用规划、写入与发布契约。
两配置各一百七十项 CPU 回归通过。
独立工具及导出项目通过移动交付检查。

教程见[资产交换](docs/runtime/asset-exchange.md)。
证据见[G21 验收](docs/acceptance/g21/2026-10-12.md)。
当前连续执行 U17 资产交换体验阶段。

"""
shared('README.md',lambda s:s.replace('# AzureRender 工程\n\n','# AzureRender 工程\n\n'+readme,1).replace('当前连续执行 G21 通用导出阶段。','音频结果按所属阶段来源维护。'))
record="""## G21 通用导出与往返 2026-10-12

新增十六种输出的注册规划与后台生成。
模型子树、动画姿态及贴图依赖通过往返检查。
图像精度、音频转换及显式覆盖恢复通过。
独立转换工具随安装交付并核验二进制来源。

证据见[G21 验收](docs/acceptance/g21/2026-10-12.md)。
正式性能、长跑及实体操作在 P8 集中验收。

"""
shared('CHANGELOG.md',lambda s:s.replace('# Changelog\n\n','# Changelog\n\n'+record,1))
plan="""## G21 自动验收结果

G21 通用导出与往返自动门禁通过。
两配置各一百七十项 CPU 回归通过。
两配置各四项受影响 GPU 回归通过。
导出制作项目完成双设备移动发布检查。

证据见[G21 验收](../acceptance/g21/2026-10-12.md)。
当前连续执行 U17 资产交换体验阶段。
正式性能、长跑及集中人工复核在 P8 验收。

"""
shared('docs/plans/azure-engine-plan.md',lambda s:s.replace('# Azure Engine 开发总计划\n\n','# Azure Engine 开发总计划\n\n'+plan,1).replace('当前连续执行 G21 通用导出阶段。','G20 结果按所属阶段来源维护。'))
p=root/'docs/plans/asset-format-manifest.json';d=json.loads(p.read_text(encoding='utf-8'))
for phase in d['phases']:
    if phase['id']=='G21':phase.update(status='AutomaticVerifiedFinalAcceptanceDeferred',automaticComplete=True,evidence='docs/acceptance/g21/evidence/2026-10-12/manifest.json')
    if phase['id']=='U17':phase['status']='Active'
for item in d['verifiedCapabilities']:
    if item['id'] in ('gltf','glb','obj','stl','ply','png','jpeg','bmp','tga','hdr','exr','tiff','webp','wav','flac','vorbis'):
        item.update(write=True,exportEvidence='docs/acceptance/g21/evidence/2026-10-12/manifest.json')
save(p,json.dumps(d,ensure_ascii=False,indent=2)+'\n')
p=root/'docs/plans/hazel-fermion-manifest.json';d=json.loads(p.read_text(encoding='utf-8'));d['closureProgress']['currentAutomaticPhase']='U17';save(p,json.dumps(d,ensure_ascii=False,indent=2)+'\n')
p=root/'docs/plans/2026-10-11-asset-format-expansion.md';s=p.read_text(encoding='utf-8').replace('状态：G20 自动门禁通过，G21 执行中','状态：G21 自动门禁通过，U17 执行中');a=s.index('## G21：');b=s.index('## U17：',a);s=s[:a]+s[a:b].replace('- [ ]','- [x]')+s[b:];save(p,s)
p=root/'docs/plans/2026-10-11-next-development.md';s=p.read_text(encoding='utf-8').replace('当前执行 G21 通用导出与往返验证。','当前执行 U17 导入导出与重导入体验。').replace('U15 至 G20 的已实施阶段自动门禁通过。','U15 至 G21 的已实施阶段自动门禁通过。').replace('证据见[G20 验收](../acceptance/g20/2026-10-12.md)。','证据见[G21 验收](../acceptance/g21/2026-10-12.md)。',1).replace('G17 至 G20 的自动门禁通过。','G17 至 G21 的自动门禁通过。').replace('G21 至 P8 按本表连续推进。','U17 至 P8 按本表连续推进。');save(p,s)
p=root/'docs/runtime/asset-formats.md';save(p,p.read_text(encoding='utf-8')+"""
## 资产输出

第一批十六种输出通过公共格式描述登记。
规划器和设置校验器由领域适配器提供。
后台任务持有注册表副本及不可变子树描述。
教程与保真范围见[资产交换](asset-exchange.md)。

""")
p=root/'docs/acceptance/g21/2026-10-12.md';s=p.read_text(encoding='utf-8').replace('状态：自动验证执行中，最终复核集中在 P8。','状态：非延期自动门禁通过，最终复核集中在 P8。').replace('两配置完整构建通过，回归和交付正在执行。','两配置完整构建及各一百七十项 CPU 回归通过。\n两配置各四项受影响 GPU 回归通过。\n移动安装、独立转换和双设备导出项目检查通过。').replace('原字节清单在全部自动门禁完成后生成。','证据见[原字节清单](evidence/2026-10-12/manifest.json)。');save(p,s)
p=root/'docs/plans/2026-10-12-asset-export-implementation.md';save(p,p.read_text(encoding='utf-8').replace('状态：执行中。','状态：非延期自动门禁通过。').replace('- [ ]','- [x]'))
print('G21 documentation and precise shared-document staging synchronized')
