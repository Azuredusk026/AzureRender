"""Synchronize R9 status using complete device and budget evidence."""
import json
from pathlib import Path
import re
import statistics
root=Path(__file__).resolve().parents[3]
work=root/'build/evolution/r9'
def read(path):return json.loads(path.read_text(encoding='utf-8'))
assert read(work/'performance/strict-budget.json')['passed']
assert read(work/'post-gates-status.json')['status']=='passed'
assert read(root/'build/ninja-msvc-release/release-gate/result.json')['status']=='passed'
for folder in ['scale-rtx','scale-intel','visibility-rtx','visibility-intel','intel','inspector-package','generated-package','installed-procedural','intel-procedural']:
 assert read(work/folder/'summary.json')['status']=='passed',folder
assert read(work/'modules-off.json')['status']=='passed'
assert read(work/'forced-validation.json')['status']=='passed'
counts={}
for name,path in [('Debug',work/'debug-tests.log'),('Release',root/'build/ninja-msvc-release/release-gate/test.log')]:
 match=re.search(r'100% tests passed, 0 tests failed out of (\d+)',path.read_text(encoding='utf-8'))
 assert match,name
 counts[name]=int(match[1])
performance=read(work/'performance/summary.json')
table=''
for load,label in [('standard','标准'),('editor','编辑器'),('stress','压力')]:
 values=[statistics.median(r[key]['p95'] for r in performance['rounds'][load]) for key in ['gpu','cpu','physics']]
 table+=f'| {label} | '+ ' | '.join(f'{value:.6f}' for value in values)+' |\n'
scale_table=''
for device,label in [('rtx','RTX'),('intel','Intel')]:
 data=read(work/('scale-'+device)/'summary.json')
 for row in data['runs']:
  scale_table+=f"| {label} | {row['instances']} | {row['variant']} | {row['gpuP95Ms']:.6f} | {row['cpuWorkP95Ms']:.6f} |\n"
text=f"""# R9 资源索引与 GPU 可见性验收

> 文档类型：阶段验收
> 日期：2026-10-07
> 状态：验收通过，资源契约采用，可见性原型保留
> 借鉴项：C3

## 公共契约与采用结论

资源访问档位通过实际功能与容量选择。
设备地址要求设备和分配器明确启用。
更新后绑定校验专属容量和池级上限。
普通固定路径使用可用的设备保证范围。

资源索引表拥有不可变资源引用与表身份。
在途替换、过期句柄与完成后释放分别验证。
角色材质的实际描述符数组消费公共索引表。
独立缓冲所有者夹具验证不同使用方式。

公共资源契约进入正式使用范围。
GPU 可见性原型保留为可选实验。
原型输出稳定间接命令与表面身份。
主绘制真实消费其间接输出。

主绘制过滤 CPU 已知不可见的提交跨度。
阴影级联各自消费独立可见跨度。
计算蒙皮按连续参数形成二维实例批次。
原生 GPU 回读验证独立骨骼与输出隔离。

CPU 可见性筛选继续参与提交。
GPU 原型的额外表面输出增加工作量。
采用结论依据两设备与规模对照。
统一收益和全面替换依据尚未成立。

## 标识、图像与生命周期

两设备各完成三种规模的独立计算对照。
间接命令逐字节一致，表面标识符合独立参考。
拾取校验实例、图元与全局材质。
场景、相机或帧身份过期时拒绝读取。

正式宿主各完成十二组规模对照。
CPU、默认、原型与固定原型图像一致。
镜像、边界、透明与深度夹具分别验证。
蒙皮、多材质和阴影由正式模型参与对照。

图像为 960×540，采用生产默认质量。
每组预热三十帧，再采样一百二十帧。
实际验证层插入，同步错误为零。
退出后原生缓冲与图像数量均为零。

规模与正式性能采集固定源码与设备。
性能采集期间 Steam 启动器正常退出。
原始失败记录随阶段证据保存。
矩阵、图像、采样与预算保持固定。

主相机可见性与阴影集合保持独立。
帧资源更新在 GPU 退休后执行。
完整回归覆盖卸载、切关与输入恢复。
模块关闭组合保持正式宿主兼容。

## 规模与性能

下表列出正式宿主的 P95，单位为毫秒。
万实例实验采用预先固定的独立预算。
具体预算与采样见运行时设计。

| 设备 | 实例 | 路径 | GPU | CPU 工作 |
| --- | --- | --- | --- | --- |
{scale_table}
九轮正式性能满足全部现行预算。
每轮预热三百帧，再采样一千八百帧。
下表为三轮 P95 中位数，单位为毫秒。

| 负载 | GPU | CPU 工作 | 物理 |
| --- | --- | --- | --- |
{table}
## 完整门禁与交付

Debug {counts['Debug']} 项与 Release {counts['Release']} 项全部通过。
发布门禁、隔离安装和双工作流移动包通过。
RTX、Intel、生成内容与既有任务均通过。
源码、工具、原始数据与包哈希形成证据链。

本阶段执行自动输入与生产操作验收。
实体键鼠复核未在本阶段执行。
私有资产和派生媒体留在本机。
历史遮挡与层级深度算法列为后续候选。

来源固定为 gkNextEngine 的参考提交。
借鉴稳定资源与表面标识的数据边界。
实现使用本项目的 RHI、帧图和生命周期。
参考提交为 `4ba5b7cd106c282e7ed166ff853aeea87b392680`。

实现见[资源访问与可见性](../../runtime/resource-visibility.md)。
操作见[可见性对照教程](../../tutorials/resource-visibility.md)。
证据见[阶段清单](evidence/2026-10-07/manifest.json)。
提交标题为 `feat(r9): 完成资源索引与可见性原型验收`。
"""
(root/'docs/acceptance/r9').mkdir(parents=True,exist_ok=True)
(root/'docs/acceptance/r9/2026-10-07.md').write_text(text,encoding='utf-8')
path=root/'docs/runtime/resource-visibility.md';path.write_text(path.read_text(encoding='utf-8').replace('实现，阶段验收进行中','实现，阶段验收通过'),encoding='utf-8')
path=root/'docs/plans/2026-10-06-engine-specialized-steps.md';text=path.read_text(encoding='utf-8');start=text.index('## R9 ');end=text.index('## G11 ')
path.write_text(text[:start]+text[start:end].replace('- [ ]','- [x]')+text[end:],encoding='utf-8')
path=root/'docs/plans/azure-engine-plan.md';text=path.read_text(encoding='utf-8')
text=text.replace('| R8，Active |','| R8，Complete |').replace('| R9，Planned |','| R9，Active |')
text=text.replace('R8 Complete，R9 Active，其后阶段 Planned','R8、R9 Complete，G11 Active，其后阶段 Planned')
text=text.replace('R7、G10、R8 已完成，当前执行阶段为 R9','R7、G10、R8、R9 已完成，当前执行阶段为 G11')
text=text.replace('## 验收环境与性能口径\n','## 验收环境与性能口径\n\nR9 的资源契约与可见性原型通过。\n'+f'Debug {counts["Debug"]} 项与 Release {counts["Release"]} 项回归通过。\n'+'两设备规模对照、九轮性能与移动包均通过。\n证据见[R9 验收](../acceptance/r9/2026-10-07.md)。\n')
path.write_text(text,encoding='utf-8')
path=root/'README.md';text=path.read_text(encoding='utf-8').replace('R7、G10、R8 已验收','R7、G10、R8、R9 已验收').replace('当前执行阶段为 R9','当前执行阶段为 G11')
text=text.replace('共享参数与可选着色策略见','资源档位与稳定表面标识见[资源可见性说明](docs/runtime/resource-visibility.md)。\n\n共享参数与可选着色策略见')
path.write_text(text,encoding='utf-8')
path=root/'CHANGELOG.md';text=path.read_text(encoding='utf-8')
path.write_text(text.replace('# Changelog\n','# Changelog\n\n## R9 资源索引与 GPU 可见性 2026-10-07\n\n'+'- 资源访问使用功能启用与实际容量共同选择。\n- 资源表支持不可变世代、在途替换与退休释放。\n- 稳定表面身份支持帧校验和拾取映射。\n- 可选 GPU 原型输出由正式间接绘制消费。\n- 两设备三种规模和固定路径图像保持一致。\n'+f'- Debug {counts["Debug"]} 项与 Release {counts["Release"]} 项回归通过。\n'+'- 九轮性能、隔离包与双工作流通过。\n'),encoding='utf-8')
path=root/'docs/plans/engine-evolution-manifest.json';manifest=read(path)
verification=dict(acceptance='docs/acceptance/r9/2026-10-07.md',evidence='docs/acceptance/r9/evidence/2026-10-07/manifest.json',adoption='verified-prototype-retained',commitTitle='feat(r9): 完成资源索引与可见性原型验收')
next(phase for phase in manifest['phases'] if phase['id']=='R9')['verification']=verification
next(item for item in manifest['items'] if item['id']=='C3')['implementation']=dict(interfaces=['DeviceCapabilities','ResourceAccessProfile','ResourceIndexTable','VisibilityPrototype','GpuSurfaceIdentity','SkinningBatch','visibleInstanceSpans'],tests=['AzureRender.ResourceAccessProfile','AzureRender.VisibilityPrototype','AzureRender.VisibilityGpu','AzureTools.VisibilityBenchmark','AzureRender.GpuCapabilityReport','AzureRender.NullRhiPass','AzureRender.SkinningBatch','AzureRender.SkinningGpu','AzureRender.ViewBatching'],verification=verification,scope='queried resource capacity and fence generations adopted; optional frustum/surface prototype; CPU filtering retained; view-owned shadow spans and independent deformation batches; Hi-Z candidate')
path.write_text(json.dumps(manifest,ensure_ascii=False,indent=2)+'\n',encoding='utf-8')
print('R9 documents synchronized from complete gate evidence')

path=root/'docs/architecture.md';text=path.read_text(encoding='utf-8')
marker='## 给 C++ 开发者的 Vulkan 入门'
section='## 资源档位与可见性\n\nRender Core 根据实际功能和容量选择资源访问。\n索引表维护表身份、世代与在途资源引用。\n普通与表面输出变体共享同一个视锥算法。\n渲染图声明主绘制对间接输出的消费。\n\n可见性查询使用独立的不可变帧映射。\n调用方声明场景、相机与帧身份。\nCPU 筛选、阴影集合与透明顺序各有归属。\n范围见[资源访问与可见性](runtime/resource-visibility.md)。\n\n'
assert marker in text
path.write_text(text.replace(marker,section+marker),encoding='utf-8')

path=root/'docs/runtime/resource-visibility.md'
text=path.read_text(encoding='utf-8')
text+='\n采用结果见[阶段验收](../acceptance/r9/2026-10-07.md)。\n'
path.write_text(text,encoding='utf-8')
