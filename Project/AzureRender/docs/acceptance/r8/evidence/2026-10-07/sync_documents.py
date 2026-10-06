"""Synchronize R8 conclusions only after complete device and budget gates."""
import json
from pathlib import Path
import re
import statistics
root=Path(__file__).resolve().parents[3]
work=root/'build/evolution/r8'
def read(path):return json.loads(path.read_text(encoding='utf-8'))
assert read(work/'performance/strict-budget.json')['passed']
assert read(root/'build/ninja-msvc-release/release-gate/result.json')['status']=='passed'
for folder in ['scene-rtx','scene-intel','intel','inspector-package','generated-package','installed-procedural','intel-procedural']:
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
 rows=performance['rounds'][load]
 values=[statistics.median(r[key]['p95'] for r in rows) for key in ['gpu','cpu','physics']]
 table+=f'| {label} | '+ ' | '.join(f'{value:.6f}' for value in values)+' |\n'
compilation=read(work/'compilation-final/compilation.json')
groups={}
for row in compilation['rounds']:
 groups.setdefault((row['language'],row.get('strategy',''),row['kind']),[]).append(row['milliseconds'])
ctable=''
for key,label in [(('glsl','','full'),'GLSL 完整'),(('slang','direct','full'),'Slang 直接完整'),
 (('slang','cached','full'),'Slang 缓存完整'),(('slang','direct','unchanged'),'Slang 直接命中'),
 (('slang','cached','unchanged'),'Slang 缓存命中'),(('slang','direct','dependency-edit'),'Slang 直接依赖编辑'),
 (('slang','cached','dependency-edit'),'Slang 缓存依赖编辑')]:
 ctable+=f'| {label} | {statistics.median(groups[key]):.3f} |\n'
gpu_table=''
for file,label in [('rtx-final-probe.json','RTX'),('intel-final-probe.json','Intel')]:
 data=read(work/file);assert data['status']=='passed' and data['pixelCases']==12 and data['released']
 gpu_table+=f'| {label} | '+' | '.join(f"{row['gpuMedianMs']:.6f}" for row in data['variants'])+' |\n'
text=f'''# R8 着色模块与算法组合验收

> 文档类型：阶段验收
> 日期：2026-10-07
> 状态：验收通过，Slang 原型保留
> 借鉴项：C2

## 契约与采用范围

着色构建工具提供版本化编译请求与结果。
共享描述生成 C++ 和 Slang 参数类型。
宿主、反射与 SPIR-V 分别检查实际布局。
公共 Bloom 算法由两种采样策略消费。

一个入口、一个算法和两个策略生成两个变体。
直接策略读取纹理，本地缓存策略保存四个样本。
接口不匹配由真实编译器拒绝。
当前类型范围为四字节标量结构。

Slang 2026.8 的来源包含编译器与动态库哈希。
动态库身份变化使缓存失效。
输入路径保护与发布异常恢复通过故障注入。
调用方串行发布，断电恢复经重新校验与构建。

## 图像、设备与生命周期

两设备分别完成 12 组真实 GPU 像素用例。
三种变体的 RGBA16F 输出逐字节一致。
独立参考覆盖半精度存储的相邻表示值。
偏移、常量跨度和结构数组跨度均通过。

正式 RenderGraph 加载三种 Bloom 程序。
RTX 与 Intel 的四帧黑洞图像逐字节一致。
相机固定为 front，质量为 balanced。
图像为 960×540，每种程序记录 120 帧计时。

验证层实际插入，同步错误为零。
夹具与宿主退出后均保持零资源。
三场景、阴影、透明、蒙皮和拾取由完整回归覆盖。
模块关闭且编译入口失效时，两宿主哈希相同。

## 编译与算法成本

下表为三轮中位数，单位为毫秒。
GLSL 项测量直接编译，Slang 项包含校验适配。
未修改的 GLSL 构建由依赖系统跳过编译。

| 情况 | 编译成本 |
| --- | --- |
{ctable}
GPU 下表为独立夹具的计时中位数。
输入尺寸为 512×512，输出为 256×256。
每个变体预热四次，再采样 32 次。
单位为毫秒，原始记录包含全部 GPU 与 CPU 样本。

| 设备 | GLSL | Slang 直接 | Slang 本地缓存 |
| --- | --- | --- | --- |
{gpu_table}
当前算法只有四次样本读取。
策略性能随设备变化，统一收益尚未成立。
采用结论为保留可选 Slang 组合原型。
共享类型契约用于现行宿主参数。

## 完整门禁与交付

Debug {counts['Debug']} 项与 Release {counts['Release']} 项全部通过。
发布门禁、隔离安装与移动内容包通过。
两种工作流和两设备均保持既有任务契约。
私有动画与派生媒体保持本机范围。

九轮正式性能满足全部预算。
每轮预热 300 帧，采样 1800 帧。
下表为三轮 P95 中位数，单位为毫秒。

| 负载 | GPU | CPU 工作 | 物理 |
| --- | --- | --- | --- |
{table}
源码、原始记录与产物哈希见[阶段清单](evidence/2026-10-07/manifest.json)。
提交标题为 `feat(r8): 完成着色模块与算法组合原型验收`。
实体键鼠复核未在本阶段执行。
正式渲染与开发重载使用现行 GLSL 程序。

设计见[着色模块与共享类型](../../runtime/shader-modules.md)。
教程见[着色模块对照](../../tutorials/shader-modules.md)。
'''
(root/'docs/acceptance/r8/2026-10-07.md').write_text(text,encoding='utf-8')
path=root/'docs/runtime/shader-modules.md'
path.write_text(path.read_text(encoding='utf-8').replace('R8，验收进行中','R8，原型验收通过'),encoding='utf-8')
path=root/'docs/plans/2026-10-06-engine-specialized-steps.md'
text=path.read_text(encoding='utf-8');start=text.index('## R8 ');end=text.index('## R9 ')
text=text[:start]+text[start:end].replace('- [ ]','- [x]')+text[end:]
path.write_text(text,encoding='utf-8')
path=root/'docs/plans/azure-engine-plan.md';text=path.read_text(encoding='utf-8')
text=text.replace('G10 Complete，R8 Active','G10、R8 Complete，R9 Active')
text=text.replace('| G10，Active |','| G10，Complete |').replace('| R8，Planned |','| R8，Active |')
text=text.replace('R7、G10 已完成，当前执行阶段为 R8','R7、G10、R8 已完成，当前执行阶段为 R9')
text=text.replace('## 验收环境与性能口径\n','## 验收环境与性能口径\n\nR8 的共享类型与策略原型通过。\n'
 f'Debug {counts["Debug"]} 项与 Release {counts["Release"]} 项回归通过。\n'
 '两设备图像、九轮性能与模块关闭组合均通过。\n证据见[R8 验收](../acceptance/r8/2026-10-07.md)。\n')
path.write_text(text,encoding='utf-8')
path=root/'README.md';text=path.read_text(encoding='utf-8')
text=text.replace('R7、G10 已验收','R7、G10、R8 已验收').replace('当前执行阶段为 R8','当前执行阶段为 R9')
text=text.replace('`shaders/` 保存 GLSL','`shaders/` 保存 GLSL 与 Slang 模块')
text=text.replace('公共能力通过探索项目和独立工具验收。','共享参数与可选着色策略见[着色模块说明](docs/runtime/shader-modules.md)。\n\n公共能力通过探索项目和独立工具验收。')
path.write_text(text,encoding='utf-8')
path=root/'CHANGELOG.md';text=path.read_text(encoding='utf-8')
path.write_text(text.replace('# Changelog\n','# Changelog\n\n## R8 着色模块与算法组合 2026-10-07\n\n'
 '- 版本化编译请求记录依赖和实际工具链指纹。\n- 唯一类型描述核对宿主、反射与二进制布局。\n'
 '- 公共 Bloom 算法支持直接和本地缓存策略。\n- 两设备夹具与正式场景图像逐字节一致。\n'
 '- 写入异常恢复、输入保护与库变更缓存失效通过。\n'
 f'- Debug {counts["Debug"]} 项与 Release {counts["Release"]} 项回归通过。\n'
 '- 九轮性能、移动安装与模块关闭组合通过。\n- Slang 组合原型按实测结果保留为可选范围。\n'),encoding='utf-8')
path=root/'docs/plans/engine-evolution-manifest.json';manifest=read(path)
verification=dict(acceptance='docs/acceptance/r8/2026-10-07.md',evidence='docs/acceptance/r8/evidence/2026-10-07/manifest.json',
 adoption='verified-prototype-retained',commitTitle='feat(r8): 完成着色模块与算法组合原型验收')
next(phase for phase in manifest['phases'] if phase['id']=='R8')['verification']=verification
next(item for item in manifest['items'] if item['id']=='C2')['implementation']=dict(
 interfaces=['ShaderCompileRequest','ShaderCompileResult','BloomParameters','IBlockSamples'],
 tests=['AzureRender.ShaderModule','AzureRender.ShaderComposition','AzureTools.ShaderModule','AzureTools.ShaderTypes','AzureTools.ShaderComposition'],
 verification=verification,scope='checked shared scalar types; optional two-strategy Bloom Slang module; default GLSL renderer')
path.write_text(json.dumps(manifest,ensure_ascii=False,indent=2)+'\n',encoding='utf-8')
print('R8 documents synchronized from complete gate evidence')
