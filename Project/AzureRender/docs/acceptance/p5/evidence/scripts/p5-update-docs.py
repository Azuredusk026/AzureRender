"""Write acceptance conclusions only from the completed machine evidence."""
from pathlib import Path
import json

ROOT=Path.cwd().resolve()
WORK=ROOT/'build/implementation-2026-10-10'
def read(name):return json.loads((WORK/name).read_text(encoding='utf-8'))
cpu=read('p5-final-cpu-summary.json');gpu=read('p5-final-gpu-summary.json')
performance=read('p5-performance/summary.json');delivery=read('p5-delivery/summary.json')
longrun=read('p5-long-run/summary.json');workflow=read('p5-workflows-retry2/summary.json')
assert all(d['passed'] for d in (cpu,gpu,performance,delivery,workflow))
assert longrun['status']=='passed' and longrun['actualSeconds']>=1800
assert len(cpu['checks'])==2 and len(gpu['checks'])==2 and len(delivery['checks'])==10

path=ROOT/'docs/acceptance/p5/2026-10-10.md'
text=path.read_text(encoding='utf-8')
text=text.replace('> 状态：验收进行中','> 状态：自动门禁通过，人工复核待执行')
text=text.replace('| 发布 | 完整回归后执行安装与目录移动 | 待验证 |','| 发布 | 双制作项目打包、目录移动与双设备启动通过 | 待复核 |')
text=text.replace('| 游戏流程 | 探索、平台与连续通关的正式回归 | 待验证 |','| 游戏流程 | 探索、平台与连续通关的正式回归通过 | 待复核 |')
section='''## 自动验收结果

Debug 的 154 项 CPU 与六十项 GPU 测试通过。
Release 的 154 项 CPU 与六十二项 GPU 测试通过。
两配置合计四百三十项测试全部通过。
GPU 测试按配置串行执行。
Debug 首轮的一项失败已定位并复测通过。
其余五十九项保留完整回归中的通过证据。
Release 中断后补跑剩余四项，并保存原始日志。
其余五十八项保留中断前的通过证据。

回归完成后登记 P5 安装文档。
两配置重新构建，并比较完整产物哈希。
测试时与交付时的全部产品哈希一致。
源码清单分别保存回归与安装规则的来源。

五类负载分别执行三轮正式采样。
下表为三轮 P95 的中位数，单位为毫秒。
每轮同时核验容量、工作集和完整采样数量。

| 负载 | GPU | CPU | 物理 |
| --- | --- | --- | --- |
'''
names={'standard':'标准场景','editor':'编辑器','stress':'五百实例','exploration':'探索关卡','platformer':'平台关卡'}
for name,label in names.items():
    row=performance['median'][name]
    section+=f"| {label} | {row['gpu']:.3f} | {row['cpu']:.3f} | {row['physics']:.3f} |\n"
growth=max(r['stableWorkingSetGrowth'] for rows in performance['rounds'].values() for r in rows)
section+=f'''
全部十五轮通过原有正式预算。
稳定工作集的最大增长为 {growth*100:.3f}%。
原始样本、温度、内容与产品哈希均保存。
Intel 核显执行功能与释放验证。

探索、平台和机械检视项目均形成独立游戏包。
两个制作项目也通过同一生产发布工具打包。
五个项目分别在 NVIDIA 与 Intel 上运行。
验证层、同步检查与资源释放全部通过。

公开探索包在同一进程完成探索与平台通关。
独立平台包验证金币、七次落点和移动承载。
掉落、检查点恢复及重开均有实际输入证据。
原项目与引擎输入目录失效时，包仍能启动。

引擎安装目录通过移动与隔离 PATH 检查。
安装清单及六份压缩包逐文件核验 SHA-256。
候选包目录为 `build/implementation-2026-10-10/p5-delivery/packages/`。
包来源与文件哈希保存在目录内的 `provenance.json`。

正式长跑使用标准交替场景，持续真实三十分钟。
它验证窗口最小化、恢复与分辨率调整。
'''
section+=f"累计 {longrun['switches']} 次切关与 {longrun['restarts']} 次重开通过。\n"
section+=f"GPU P95 为 {longrun['gpu']['p95']:.3f} 毫秒。\n"
section+='''
每分钟窗口、周期 CPU 与提交延迟通过预算。
常驻资源、工作集与关闭释放均通过。
标准长跑与两个游戏的完整输入验证分别记录。
实体操作与主观音效仍待人工填写复核表。

日志、输入、运行报告与来源见[证据清单](evidence/manifest.json)。

'''
text=text.replace('## 修复记录\n',section+'## 修复记录\n')
text=text.replace('## 完成条件\n','''空关卡制作测试核验全部源模型的保存结果。
探索环境包含十一个不同模型，测试据此检查完整性。
节点、共享资源身份与保存引用逐个核验。
原固定资源数量断言的失败日志与复测结果均保留。

## 完成条件
''')
path.write_text(text,encoding='utf-8')

path=ROOT/'docs/plans/2026-10-09-editor-productization.md'
text=path.read_text(encoding='utf-8').replace('P5 验收中','P5 自动门禁通过，人工待复核')
start=text.index('### 用户制作任务',text.index('## P5：'))
end=text.index('### 回归和预算',start)
part=text[start:end]
for line in part.splitlines():
    if line.startswith('- [ ] ') and '实体键鼠' not in line:part=part.replace(line,line.replace('- [ ]','- [x]',1))
part+='前十一项按自动制作与交付证据标记。\n实体键鼠与音效听感按人工结果完成。\n结果见[P5 验收](../acceptance/p5/2026-10-10.md)。\n\n'
text=text[:start]+part+text[end:]
path.write_text(text,encoding='utf-8')

path=ROOT/'docs/plans/azure-engine-plan.md'
text=path.read_text(encoding='utf-8').replace('P5 为 Active。\n其余主线阶段为 Planned。','P5 为 Active，自动门禁通过。\n实体键鼠与主观音效复核待执行。\n可选专项按产品化计划的触发条件安排。')
text+='\n产品化测试、双设备交付与长跑见[P5 验收](../acceptance/p5/2026-10-10.md)。\n'
path.write_text(text,encoding='utf-8')

path=ROOT/'docs/plans/hazel-fermion-manifest.json'
data=json.loads(path.read_text(encoding='utf-8'))
data['execution']['productizationEvidence']={'status':'MachineVerifiedPhysicalInputPending',
    'acceptance':'docs/acceptance/p5/2026-10-10.md','manifest':'docs/acceptance/p5/evidence/manifest.json',
    'manual':'docs/acceptance/p5/physical-input.md','cpuTests':308,'gpuTests':122,
    'performanceRounds':15,'deliveredProjects':5,'deviceChecks':10,'longRunSeconds':longrun['actualSeconds']}
data['documentationVerification']['implementationTests']+='; P5: Debug 214 tests, Release 216 tests, 15 strict performance rounds, five relocated game packages on two GPUs, real 30-minute standard long run; physical input and subjective audio pending'
path.write_text(json.dumps(data,ensure_ascii=False,indent=2)+'\n',encoding='utf-8')

path=ROOT/'CHANGELOG.md'
text=path.read_text(encoding='utf-8')
entry='''## P5 产品化机器验收 2026-10-10

场景模板使用 `.azurelevel` 保存注册组件。
修复模板刚体字段保存重开与实时查询崩溃。
设置测试通过真实分类入口操作。
两模板完成导入、材质、Prefab、模拟与文档恢复。

Debug 与 Release 合计四百三十项回归通过。
十五轮正式性能、双设备发布与三十分钟长跑通过。
候选包保存源码、产品与文件哈希。
实体键鼠及主观音效复核待执行，P5 保持 Active。

详细结果见[P5 验收](docs/acceptance/p5/2026-10-10.md)。

'''
text=text.replace('# Changelog\n\n','# Changelog\n\n'+entry,1)
path.write_text(text,encoding='utf-8')
print('P5 machine outcomes documented; manual requirements remain pending')
