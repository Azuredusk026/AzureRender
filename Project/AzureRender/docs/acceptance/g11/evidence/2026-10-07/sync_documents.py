"""Close G11 documentation from complete, current gate evidence."""
import json
from pathlib import Path
import re
import statistics
root=Path(__file__).resolve().parents[3];work=root/'build/evolution/g11'
def read(path):return json.loads(path.read_text(encoding='utf-8'))
assert read(work/'post-gates-status.json')['status']=='passed'
assert read(work/'modules-off.json')['status']=='passed'
assert read(work/'performance/strict-budget.json')['passed']
counts={}
for name,path in [('Debug',work/'debug-tests.log'),('Release',root/'build/ninja-msvc-release/release-gate/test.log')]:
    match=re.search(r'100% tests passed, 0 tests failed out of (\d+)',path.read_text(encoding='utf-8'));assert match,name
    counts[name]=int(match[1])
performance=read(work/'performance/summary.json')
table=''
for load,label in [('standard','标准'),('editor','编辑器'),('stress','压力')]:
    values=[statistics.median(r[key]['p95'] for r in performance['rounds'][load]) for key in ['gpu','cpu','physics']]
    table+=f'| {label} | '+' | '.join(f'{value:.6f}' for value in values)+' |\n'
metrics=[]
for config in ('debug','release'):
    for line in (work/(config+'-last-test.log')).read_text(encoding='utf-8',errors='replace').splitlines():
        if line.startswith('{'):
            try:data=json.loads(line)
            except ValueError:continue
            if 'stepMsPerIteration' in data:metrics.append(dict(configuration=config,**data))
assert len(metrics)==6,'Three real backend measurements required per configuration'
(work/'backend-measurements.json').write_text(json.dumps(metrics,indent=2)+'\n',encoding='utf-8')
metric_table=''
for row in metrics:
    metric_table+=f"| {row['configuration']} | {row['backend']} | {row['startupMs']:.6f} | {row['stepMsPerIteration']:.6f} |\n"
build=read(root/'build/ninja-msvc-release/managed/manifest.json')
text=f'''# G11 脚本服务与生成绑定验收

> 文档类型：阶段验收
> 日期：2026-10-07
> 状态：验收通过，公共契约采用，托管原型保留
> 借鉴项：C4

## 契约与采用范围

`IScriptRuntime` 由运行时核心提供。
Player 与编辑预览通过同一装配选择后端。
Lua、CoreCLR 和 NativeAOT 共享绑定宿主。
语言依赖由各自构建目标拥有。

绑定描述生成十五项操作与只读节点标识。
生成检查和握手共同核对版本与描述哈希。
实体编号、世代与场景身份守卫对象访问。
公共服务与唯一描述进入正式采用范围。

两托管后端保留为可选原型，默认服务为 Lua。
CoreCLR 使用真实 hostfxr 和可回收上下文。
NativeAOT 使用真实发布库与直接类型注册。
它要求重建模块并启动新进程。

## 生命周期与失败行为

初始化暂存组件和延迟宿主效果。
失败候选保留活动实例、属性与结构队列。
成功提交后才执行旧实例关闭回调。
候选属性保持交接时的写入优先级。

三个后端验证真实运行回调异常隔离。
出错对象停止，其他对象继续执行。
中文字符串、数组与物理事件通过公共绑定。
场景替换和删除使过期对象失效。

版本化 ABI 声明尺寸与字节所有权。
一 MiB 输出容量在效果执行前检查。
保存的回调在关闭及异线程访问时被拒绝。
关闭释放会话、候选和脚本对象。

CoreCLR 的弱引用与实际回收验证上下文卸载。
托管入口及原生模块映射保留至进程退出。
托管扩展作为受信任编译内容运行。
原型支持一个用户程序集及引擎 API 引用。

## 实测成本

构建采用 SDK {build['sdk']} 与运行库 9.0.11。
清单保存真实命令、耗时、输出字节及哈希。
下表为服务装配与一千次固定步调用的测量。
单位为毫秒，固定步包含实际组件写入。

| 配置 | 后端 | 服务启动 | 每次固定步 |
| --- | --- | --- | --- |
{metric_table}
托管产物体积与构建耗时见证据清单。
缓存构建耗时按该次运行记录解释。
NativeAOT 的分发体积和编译要求进入采用判断。
核心运行库条件由教程明确说明。

## 完整验收与性能

Debug {counts['Debug']} 项与 Release {counts['Release']} 项全部通过。
两后端在 RTX 和 Intel 验证实际 Vulkan 工作流。
场景标注与角色控制分别验证机制复用。
编辑预览、停止恢复和移动发布包全部通过。

独立模块关闭构建在缺编译器条件下通过。
发布包验证隔离环境、源目录隐藏与篡改拒绝。
CoreCLR 使用已安装运行库。
NativeAOT 在不可用运行库路径下启动。

九轮正式性能使用冻结源码与产物。
每轮预热三百帧，采样一千八百帧。
全部现行时间与内存预算通过。
下表为三轮 P95 中位数，单位为毫秒。

| 负载 | GPU | CPU 工作 | 物理 |
| --- | --- | --- | --- |
{table}
运行报告按显式请求记录角色回放状态。
GPU 测量报告提供保留帧数，便于核对开销。
显式运行报告的逐帧数据与输入回放保持完整。
工作集超限的原始记录随阶段证据保存。

实际验证层与同步检查的错误数为零。
双设备独立蒙皮与既有任务回归通过。
实体键鼠复核未在本阶段执行。
自动输入与生产操作的记录独立保存。

参考提交为 `4ba5b7cd106c282e7ed166ff853aeea87b392680`。
借鉴内容为脚本服务、描述生成与加载边界。
实现与测试使用本项目的组件及生命周期。
原始失败、复测、来源与许可均进入证据链。

契约见[脚本服务](../../runtime/script-services.md)。
教程见[托管脚本工作流](../../tutorials/managed-scripts.md)。
证据见[阶段清单](evidence/2026-10-07/manifest.json)。
提交标题为 `feat(g11): 完成脚本服务与生成绑定原型验收`。
'''
path=root/'docs/acceptance/g11/2026-10-07.md';path.parent.mkdir(parents=True,exist_ok=True);path.write_text(text,encoding='utf-8')
path=root/'docs/plans/g11-implementation.md';text=path.read_text(encoding='utf-8').replace('状态：执行中','状态：验收通过').replace('- [ ]','- [x]');path.write_text(text,encoding='utf-8')
path=root/'docs/plans/2026-10-06-engine-specialized-steps.md';text=path.read_text(encoding='utf-8');a=text.index('## G11 ');b=text.index('## P3 ');path.write_text(text[:a]+text[a:b].replace('- [ ]','- [x]')+text[b:],encoding='utf-8')
for path in [root/'docs/plans/azure-engine-plan.md',root/'README.md',root.parent.parent/'README.md']:
    text=path.read_text(encoding='utf-8').replace('R9 Complete，G11 Active，其后阶段 Planned','R9、G11 Complete，P3 Active')
    text=text.replace('| R9，Active |','| R9，Complete |').replace('| G11，Planned |','| G11，Active |')
    text=text.replace('当前执行阶段为 G11','当前执行阶段为 P3').replace('R7、G10、R8、R9 已完成','R7、G10、R8、R9、G11 已完成')
    text=text.replace('当前执行阶段为 R8 着色模块与算法组合','当前执行阶段为 P3 引擎复用与交付验收')
    if path.name=='azure-engine-plan.md':text=text.replace('## 验收环境与性能口径\n','## 验收环境与性能口径\n\nG11 公共脚本服务与生成绑定通过。\n两托管原型、模块关闭与移动发布均通过。\n证据见[G11 验收](../acceptance/g11/2026-10-07.md)。\n')
    path.write_text(text,encoding='utf-8')
path=root/'CHANGELOG.md';text=path.read_text(encoding='utf-8');path.write_text(text.replace('# Changelog\n','# Changelog\n\n## G11 脚本服务与生成绑定 2026-10-07\n\n- 脚本服务通过注册装配，语言依赖归属独立模块。\n- 唯一描述生成公共操作与托管包装，握手核对哈希。\n- 初始化事务支持候选提交与实例交接。\n- 托管原型验证真实加载、发布、隔离与关闭。\n- 两配置、双设备、模块关闭与九轮性能通过。\n'),encoding='utf-8')
path=root/'docs/plans/engine-evolution-manifest.json';manifest=read(path)
verification=dict(acceptance='docs/acceptance/g11/2026-10-07.md',evidence='docs/acceptance/g11/evidence/2026-10-07/manifest.json',adoption='verified-prototype-retained',commitTitle='feat(g11): 完成脚本服务与生成绑定原型验收')
next(p for p in manifest['phases'] if p['id']=='G11')['verification']=verification
next(i for i in manifest['items'] if i['id']=='C4')['implementation']=dict(interfaces=['IScriptRuntime','ScriptRuntimeRegistry','ScriptBindingHost','ScriptHostApi','ScriptHostSession','ManagedScriptRuntime'],tests=['AzureTools.ScriptBindings','AzureEngine.ScriptBackendContract','AzureEngine.ScriptInterop','AzureEngine.ScriptRuntime','AzureEngine.ManagedModule.coreclr','AzureEngine.ManagedModule.nativeaot','AzureEngine.ManagedRuntime.coreclr','AzureEngine.ManagedRuntime.nativeaot','AzureEngine.ScriptBackendParity.lua','AzureEngine.ManagedWorkflows.coreclr','AzureEngine.ManagedWorkflows.nativeaot'],verification=verification,scope='shared service and generated description adopted; optional trusted CoreCLR and NativeAOT prototypes; Lua default')
path.write_text(json.dumps(manifest,ensure_ascii=False,indent=2)+'\n',encoding='utf-8')
print('G11 documents synchronized from complete gate evidence')
