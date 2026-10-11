from pathlib import Path
import hashlib,json,subprocess
root=Path.cwd();out=root/'build/g19-dependency-evaluation-2026-10-12'
for name,count in [('debug-cpu.log',166),('release-cpu.log',166),('debug-gpu.log',13),('release-gpu.log',13)]:
    assert f'100% tests passed, 0 tests failed out of {count}' in (out/name).read_text(encoding='utf-8'),name
for config in ('Debug','Release'):
    report=json.loads((out/'image-render'/config/'summary.json').read_text(encoding='utf-8'));assert report['passed'] and len(report['cases'])==22
baseline=json.loads((out/'document-baseline.json').read_text(encoding='utf-8'))
def save(path,text):
    temp=path.with_suffix(path.suffix+'.g19tmp');temp.write_text(text,encoding='utf-8');temp.replace(path)
def shared(name,transform):
    entry=baseline[name];assert (root/name).read_text(encoding='utf-8')==entry['work'],name
    save(root/name,transform(entry['work']))
    blob=subprocess.check_output(['git','hash-object','-w','--stdin'],input=transform(entry['head']).encode()).decode().strip()
    subprocess.run(['git','update-index','--cacheinfo','100644,'+blob+',Project/AzureRender/'+name],check=True)
readme='''G19 图像格式与精度语义通过自动验收。
九种输入共用材质、预览及环境解码接口。
两配置各一百六十六项 CPU 回归通过。
两配置各二十二组新图像双设备渲染通过。

指南见[图像导入](docs/runtime/image-format-import.md)。
证据见[G19 验收](docs/acceptance/g19/2026-10-12.md)。
当前连续执行 G20 音频格式阶段。

'''
shared('README.md',lambda s:s.replace('# AzureRender 工程\n\n','# AzureRender 工程\n\n'+readme,1).replace('当前连续执行 G19 图像格式阶段。','模型结果按所属阶段来源维护。'))
changelog='''## G19 图像解码与精度语义 2026-10-12

新增九种图像输入的公共注册与解码接口。
保留十六位及浮点精度，记录材质转换损失。
修复十六位 PNM 字节序与线性法线贴图转换。
环境亮度、中文图片导入及移动发布通过。

阶段证据见[G19 验收](docs/acceptance/g19/2026-10-12.md)。
Intel 地面像素的重复捕获波动保留对照记录。
正式性能、长跑和人工复核在 P8 执行。

'''
shared('CHANGELOG.md',lambda s:s.replace('# Changelog\n\n','# Changelog\n\n'+changelog,1))
plan='''## G19 自动验收结果

G19 图像解码与精度语义自动门禁通过。
两配置各一百六十六项 CPU 回归通过。
两配置各二十二组新图像双设备渲染通过。
两配置各十三项受影响 GPU 回归通过。

证据见[G19 验收](../acceptance/g19/2026-10-12.md)。
当前连续执行 G20 音频格式阶段。
P5 待项、正式性能与人工复核在 P8 验收。

'''
shared('docs/plans/azure-engine-plan.md',lambda s:s.replace('# Azure Engine 开发总计划\n\n','# Azure Engine 开发总计划\n\n'+plan,1).replace('当前连续执行 G19 图像格式阶段。','G18 结果按所属阶段来源维护。').replace('P5 为 Active。','P5 的剩余验收在 P8 集中执行。').replace('该计划状态为 Active，P5 保持 Active。','该计划持续执行，P5 待项由 P8 验收。'))
p=root/'docs/plans/asset-format-manifest.json';d=json.loads(p.read_text(encoding='utf-8'))
for phase in d['phases']:
    if phase['id']=='G19':phase.update(status='AutomaticVerifiedFinalAcceptanceDeferred',automaticComplete=True,evidence='docs/acceptance/g19/evidence/2026-10-12/manifest.json')
    if phase['id']=='G20':phase['status']='Active'
save(p,json.dumps(d,ensure_ascii=False,indent=2)+'\n')
p=root/'docs/plans/hazel-fermion-manifest.json';d=json.loads(p.read_text(encoding='utf-8'));d['closureProgress']['currentAutomaticPhase']='G20';d['closureProgress']['gpuSchedule']='Sequential functional GPU verification; formal performance and long runs in P8';save(p,json.dumps(d,ensure_ascii=False,indent=2)+'\n')
p=root/'docs/plans/2026-10-11-asset-format-expansion.md';s=p.read_text(encoding='utf-8').replace('状态：G18 自动门禁通过，G19 执行中','状态：G19 自动门禁通过，G20 执行中')
a=s.index('## G19：');b=s.index('## G20：',a);s=s[:a]+s[a:b].replace('- [ ]','- [x]')+s[b:]
s=s.replace('| 图像登记 | PNG、JPEG、PPM、HDR |','| 图像登记 | 九种第一批输入格式 |').replace('| 材质图像 | stb 解码，普通贴图存储八位像素 |','| 材质图像 | 统一精度解码，按槽转换为八位像素 |').replace('| 环境图像 | EXR/HDR 浮点解码与其他图像解码 |','| 环境图像 | 统一解码，保持线性半精度范围 |')
save(p,s)
p=root/'docs/plans/2026-10-11-next-development.md';s=p.read_text(encoding='utf-8').replace('U15、U16、P6、G17 与 G18 的自动门禁通过。','U15 至 G19 的已实施阶段自动门禁通过。').replace('当前执行 G19 图像解码与精度语义。','当前执行 G20 音频解码与资源生命周期。').replace('证据见[G18 验收](../acceptance/g18/2026-10-12.md)。','证据见[G19 验收](../acceptance/g19/2026-10-12.md)。',1)
s=s.replace('当前 P5、U15、U16 与 P6 的必需门禁仍有待项。\n格式阶段 G17 至 P8 均保持计划状态。','P5 剩余条件及集中人工复核在 P8 验收。\nG17、G18 与 G19 的自动门禁通过。\nG20 至 P8 按本表连续推进。')
save(p,s)
p=root/'docs/runtime/asset-formats.md';s=p.read_text(encoding='utf-8');s+='''
## 图像输入

图像格式通过同一只读注册与传输服务接入。
PNG、JPEG、BMP、TGA、PPM、HDR、EXR、TIFF、WebP 可读取。
材质、预览与环境共用精度及颜色描述。
转换范围见[图像导入](image-format-import.md)。

图片复制容器并校验实际像素。
图像导入摘要记录精度、方向及材质损失。
纹理重导入校验设置、解码与 mip。
图像标识沿用内容事务的保存与发布契约。
''';save(p,s)
record='''# G19 图像格式与精度验收

> 日期：2026-10-12。
> 状态：非延期自动门禁通过，最终验收在 P8 执行。

## 公共实现

九种图像输入通过资产格式注册接入。
统一解码器属于 RenderCore 资产领域。
材质、预览、环境及重导入使用同一像素描述。
图像传输沿用有界复制、指纹及受控发布。

八位、十六位和浮点像素分别保留存储精度。
数据使用直通透明与左上起始 RGBA 行序。
材质按颜色或数据槽转换并记录损失。
环境保留线性范围，校验半精度有限上限。

来源大小、尺寸及像素预算在分配前校验。
TIFF 解码和诊断状态由单次调用拥有。
EXR 偏移窗口通过安全切片接口读取。
来源与许可见[图像依赖清单](../../licenses/image-codecs-source.json)。

## 实际验证

Debug 与 Release 各一百六十六项 CPU 通过。
九格式实际导入、校验及发布通过。
透明、灰度、十六位 PNG 与 PNM 数值通过。
TIFF 方向、浮点范围及 EXR 负窗口通过。

截断、非法容器、取消与超限尺寸分别拒绝。
中文图片经公共导入、重导入及保存重开通过。
隐藏来源后的资源包解析通过。
glTF 的 WebP 与线性法线数据接入通过。

两配置各二十二组新图像双设备渲染通过。
PNG 与 WebP 的受影响材质区域像素全等。
两配置各十三项既有 GPU 回归通过。
采样、材质、环境、角色及游戏串联均有记录。

验证层与同步检查未报告错误。
捕获退出的缓冲与图像计数归零。
移动安装版在隔离环境渲染全部新增夹具。
安装清单、运行库和第三方许可逐字节核验。

## 重复捕获对照

Intel 首次全帧比较有两个地面像素相差一级。
相同 PNG 的重复捕获也出现少量一级差值。
首帧贴图区域在三轮对照中保持全等。
整帧验收沿用已有视觉回归预算。

后续帧的差值至多为两个色值等级。
原始失败、重复捕获及差异坐标均保留。
三轮同格式对照记录独立地面材质的波动。
格式结果与整帧渲染变化分别说明覆盖范围。
记录见[阶段证据](evidence/2026-10-12/manifest.json)。

## 阶段边界

操作者键鼠与使用理解性在 P8 集中复核。
P5 归因、正式性能及两类长跑保持 P8 门禁。
原预算、失败记录与最终来源要求持续有效。
当前顺序进入 G20 音频格式阶段。

接口说明见[图像导入指南](../../runtime/image-format-import.md)。
'''
p=root/'docs/acceptance/g19/2026-10-12.md';p.parent.mkdir(parents=True,exist_ok=True);save(p,record)
files=['docs/plans/asset-format-manifest.json','docs/plans/hazel-fermion-manifest.json','docs/plans/2026-10-11-asset-format-expansion.md','docs/plans/2026-10-11-next-development.md','docs/runtime/asset-formats.md','docs/acceptance/g19/2026-10-12.md']
subprocess.run(['git','add','--',*files],check=True)
print('G19 documentation synchronized; shared user changes remain unstaged')
