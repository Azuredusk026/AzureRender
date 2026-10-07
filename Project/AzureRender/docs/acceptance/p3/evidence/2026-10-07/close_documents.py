from pathlib import Path
import json,re,statistics
root=Path(__file__).resolve().parents[3];work=root/'build/evolution/p3'
def read(p):return json.loads(p.read_text(encoding='utf-8-sig'))
assert read(work/'gate-status.json')['status']=='passed'
assert read(work/'authoring-reopened/summary.json')['status']=='passed'
long=read(work/'long-run/summary.json');assert long['status']=='passed' and long['actualSeconds']>=1800
perf=read(work/'performance/strict-budget.json');assert perf['passed']
coverage=read(work/'coverage/coverage.json');assert coverage['coveredItems']==18
source=read(work/'source-snapshot.json');assert source['status']=='passed'
counts={}
for config,p in [('Debug',work/'debug-tests.log'),('Release',root/'build/ninja-msvc-release/release-gate/test.log')]:
 m=re.search(r'100% tests passed, 0 tests failed out of (\d+)',p.read_text(encoding='utf-8',errors='replace'));assert m;counts[config]=int(m[1])
summary=read(work/'performance/summary.json');table=''
for load,label in [('standard','标准'),('editor','编辑器'),('stress','压力')]:
 values=[statistics.median(r[key]['p95'] for r in summary['rounds'][load]) for key in ('gpu','cpu','physics')]
 table+='| '+label+' | '+' | '.join(f'{v:.6f}' for v in values)+' |\n'
byte=coverage['historicalByteAudit'];record=root/'docs/acceptance/p3/2026-10-07.md';assert not record.exists();record.parent.mkdir(parents=True,exist_ok=True)
commits=''
for phase in coverage['phases']:
 commits+='| '+phase['phase']+' | `'+phase['commit'][:7]+'` | 功能与交付门禁通过 |\n'
text=f'''# P3 引擎复用与独立交付验收

> 文档类型：阶段验收
> 日期：2026-10-07
> 状态：功能与交付验收通过，历史字节限制明确保留
> 范围：十八项借鉴与十二个实施阶段

## 引擎能力与采用结论

十八项均关联真实接口、任务、测试与阶段提交。
六项核心契约覆盖模块、类型和统一操作。
八项工具能力覆盖 UI、模型、观察与开发服务。
四项专项分别保留经验证的采用范围。

探索游戏与场景检视通过项目配置组合公共机制。
专用任务和标注行为归应用脚本及内容。
Player 保持独立于编辑器实现。
接口源码快照和模式交付四十二个实际文件。

接口快照用于契约阅读及模式消费。
通用预编译 C++ SDK 属于后续专项范围。
默认脚本为 Lua，托管模块为可选原型。
托管扩展按受信任编译内容管理。

## 阶段与提交

阶段按正式计划依次实施并独立提交。
各项采用范围与历史字节限制随证据记录。

| 阶段 | 提交 | 验收结论 |
| --- | --- | --- |
{commits}| P3 | 按本页提交标题定位 | 功能与交付通过，历史字节限制保留 |

## 完整回归与独立工作流

Debug {counts['Debug']} 项与 Release {counts['Release']} 项全部通过。
三场景视觉及既有任务回归通过。
RTX 与 Intel 验证蒙皮、脚本和实际渲染。
关闭托管及着色原型的独立构建通过。

探索游戏在移动包中完成完整任务。
二十次切关与二十次重新开始通过。
场景检视包移入含空格路径并独立运行。
两种工作流验证输入隐藏与隔离资源定位。

空项目从零对象与零资源完成关卡制作。
生产操作验证撤销、保存和独立构建。
独立编辑器进程重开后验证相同场景字节。
Play/Stop 恢复与独立包校验通过。

安装中的接口快照验证完整清单与文件哈希。
游戏包验证许可、入口与篡改拒绝。
正常退出后分配器中的图像和缓冲均为零。
实际验证层及同步检查的错误数为零。

## 正式性能与真实长跑

九轮性能使用冻结源码与产物。
每轮预热三百帧，采样一千八百帧。
现行时间、容量和工作集增长预算全部通过。
下表为三轮 P95 中位数，单位为毫秒。

| 负载 | GPU | CPU 工作 | 物理 |
| --- | --- | --- | --- |
{table}
真实长跑持续 {long['actualSeconds']:.3f} 秒。
标准负载包含一百实体、四个动画角色和八点光。
长跑覆盖持续移动、预加载、切关及重新开始。
窗口最小化、恢复、缩放与正常关闭通过。

长跑 GPU、CPU 与物理的整体预算通过。
分分钟 GPU 窗口及周期 CPU 采样均达标。
驻留资源、工作集和容量预算全部通过。
真实时长与原始逐帧记录分别保存。

长跑回放按八千一百九十二动作预算生成。
每个周期保留完整操作与按键释放。
生成器缺陷的失败记录与修复复测均归档。

Lua 对象代理引用由垃圾回收可见的闭包持有。
连续查询与自定义字段通过生命周期回归。
内存诊断、部分长跑及修复前源码均保存。
完整验收针对最终冻结版本执行。

一次压力测量的工作集增长为 13.24%。
五轮同条件诊断的增长为 1.24% 至 1.60%。
该次突增的具体分配来源仍未确定。
原始失败、诊断及完整九轮复测分别保存。

最终九轮与真实长跑各自满足现行预算。
孤立突增保留为可复现性风险。
风险记录见 `working-set-investigation.json`。

一次完整长跑的工作集增长为 5.25%。
隔离诊断确认资源遥测的 JSON 存储持续增长。
类型化环形记录保留相同容量、采样及字段。
八千一百九十二条记录的内存回归通过。

三十分钟失败记录及对应冻结源码完整保留。
最终版本重新执行两配置与全部交付门禁。

## 来源、证据与限制

源码快照保存 {source['files']} 个文件的原始字节。
全部 {source['productionInputs']} 项生产输入与两配置来源一致。
源码归档内部文件及压缩包均通过 SHA-256 核对。
公开素材、第三方许可与独立包各有清单。

超过三十二 MiB 的运行记录使用 gzip 保存。
证据清单同时记录压缩字节与原始字节哈希。
原始记录经解压后逐项核对。
源码快照保留逐文件原始字节。

历史证据的归档字节与阶段提交均通过核对。
历史输入中 {byte['exactInputs']} 项可还原到原始哈希。
另有 {len(byte['unverified'])} 项原始字节无法从 Git 内容还原。
这些输入的字节等价仍属于明确审计限制。

严格历史字节核查保存实际失败记录。
接口覆盖通过与历史字节审计分别报告。
各阶段的原始哈希及证据保持完整。
本次交付源码通过独立原始字节快照核对。

实体键鼠复核未在本阶段执行。
自动输入及生产操作分别保存证据。
CoreCLR 依赖外部运行库，NativeAOT 重建后重启。
平台、模型与专项原型的范围见采用清单。

采用记录见[实施清单](../../plans/engine-evolution-manifest.json)。
原始证据随完整 Git 仓库提供。
清单位于 `docs/acceptance/p3/evidence/2026-10-07/manifest.json`。

交付规则见[引擎交付](../../runtime/engine-delivery.md)。
教程见[引擎复用工作流](../../tutorials/engine-reuse.md)。
提交标题为 `feat(p3): 完成引擎复用与独立交付验收`。
'''
record.write_text(text,encoding='utf-8')
p=root/'docs/plans/p3-implementation.md';s=p.read_text(encoding='utf-8').replace('状态：执行中','状态：功能与交付验收通过，历史字节限制保留').replace('- [ ]','- [x]');p.write_text(s,encoding='utf-8')
p=root/'docs/plans/2026-10-06-engine-specialized-steps.md';s=p.read_text(encoding='utf-8');a=s.index('## P3 ');s=s[:a]+s[a:].replace('- [ ]','- [x]');p.write_text(s,encoding='utf-8')
for p in [root/'README.md',root.parent.parent/'README.md',root/'docs/plans/azure-engine-plan.md']:
 s=p.read_text(encoding='utf-8').replace('G11 Complete，P3 Active','G11、P3 Complete').replace('当前执行阶段为 P3 引擎复用与交付验收','F4 至 P3 的功能与交付验收已完成').replace('F4、G8、U3、F5、U4、G9、R7、G10、R8、R9、G11 已完成，当前执行阶段为 P3。','F4 至 P3 的功能与交付验收已完成。').replace('当前执行阶段为 P3。','F4 至 P3 的功能与交付验收已完成。')
 s+='\n引擎交付与限制见[P3 验收]('+('Project/AzureRender/docs/acceptance/p3/2026-10-07.md' if p==root.parent.parent/'README.md' else ('docs/acceptance/p3/2026-10-07.md' if p==root/'README.md' else '../acceptance/p3/2026-10-07.md'))+')。\n'
 p.write_text(s,encoding='utf-8')
p=root/'docs/plans/azure-engine-plan.md';s=p.read_text(encoding='utf-8').replace('更新日期：2026-10-06','更新日期：2026-10-07');p.write_text(s,encoding='utf-8')
p=root/'docs/plans/2026-10-06-engine-evolution.md';s=p.read_text(encoding='utf-8').replace('- [ ]','- [x]');p.write_text(s,encoding='utf-8')
p=root/'docs/index.md';s=p.read_text(encoding='utf-8').replace('F4、G8、U3、F5、U4、G9、R7、G10 已验收，当前执行阶段为 R8。','F4 至 P3 的功能与交付验收已完成。');p.write_text(s,encoding='utf-8')
p=root/'CHANGELOG.md';s=p.read_text(encoding='utf-8');s=s.replace('# Changelog\n','# Changelog\n\n## P3 引擎复用与独立交付 2026-10-07\n\n- 十八项借鉴关联实际接口、任务、测试与阶段提交。\n- 构建及安装交付可核对的接口源码快照和模式。\n- 两种移动工作流、空项目重开及独立构建通过。\n- Lua 对象代理引用进入可追踪的垃圾回收闭包。\n- 连续对象查询的内存回归及长跑生成器复测通过。\n- 两配置、双设备、九轮性能和真实三十分钟长跑通过。\n- 资源遥测采用类型化环形记录并逐条输出 JSON。\n- 原有采样、容量和报告字段通过完整回归。\n- 本次源码保存原始字节，历史输入字节限制单独报告。\n');p.write_text(s,encoding='utf-8')
p=root/'mkdocs.yml';s=p.read_text(encoding='utf-8').replace('  - 项目总览: index.md','  - 项目总览: index.md\n  - P3 引擎交付验收: acceptance/p3/2026-10-07.md');s=s.replace('  archive/','  archive/\n  **/evidence/**/source-snapshot/**');p.write_text(s,encoding='utf-8')
(work/'documents-closed.json').write_text(json.dumps({'status':'passed','tests':counts,'historicalByteLimits':len(byte['unverified'])},indent=2)+'\n')
print('P3 documentation synchronized from final verified evidence',flush=True)
