"""Record completed delivery and real-time observations while retaining the failed gate."""
import json
from pathlib import Path
ROOT=Path.cwd();WORK=ROOT/'build/implementation-2026-10-10'
longrun=json.loads((WORK/'p5-long-run/summary.json').read_text(encoding='utf-8'))
assert longrun['actualSeconds']>=1800
p=ROOT/'docs/acceptance/p5/2026-10-10.md';t=p.read_text(encoding='utf-8')
if longrun['status']=='passed':
 s=f"真实三十分钟标准交替场景长跑通过。\n累计 {longrun['frames']} 帧。\n完成 {longrun['switches']} 次切关与 {longrun['restarts']} 次重开。\nGPU P95 为 {longrun['gpu']['p95']:.3f} 毫秒。\n\n每分钟 GPU、周期 CPU 和提交延迟通过预算。\n工作集增长为 {longrun['workingSetGrowth']*100:.3f}%。\n常驻资源增长和退出释放通过预算。\n窗口最小化、恢复与分辨率调整均通过。\n"
else:
 s=f"真实三十分钟标准交替场景长跑完成测量。\n最终门禁未通过，原始报告保留失败字段。\n累计 {longrun['frames']} 帧。\nGPU P95 为 {longrun['gpu']['p95']:.3f} 毫秒。\n"
t=t.replace('真实三十分钟长跑正在执行。\n',s+'\n')
t=t.replace('## 完成条件\n','日志、输入、来源与失败记录见[证据清单](evidence/manifest.json)。\n\n## 完成条件\n');p.write_text(t,encoding='utf-8')
p=ROOT/'docs/plans/hazel-fermion-manifest.json';d=json.loads(p.read_text(encoding='utf-8'));d['execution']['productizationEvidence']['longRun']=dict(status=longrun['status'],seconds=longrun['actualSeconds'],frames=longrun['frames'],gpuP95Ms=longrun['gpu']['p95']);d['documentationVerification']['implementationTests']+='; P5: 430 distinct regression items passed with retained interruption/retest logs; five relocated projects on two GPUs; working-set performance gate failed; physical input and subjective audio pending';p.write_text(json.dumps(d,ensure_ascii=False,indent=2)+'\n',encoding='utf-8')
p=ROOT/'CHANGELOG.md';t=p.read_text(encoding='utf-8');entry="""## P5 制作、发布与验收记录 2026-10-10

场景模板使用 `.azurelevel` 保存注册组件。
修复模板刚体字段保存重开与实时查询崩溃。
设置分类提供一致的控件观察标识。
两模板完成导入、材质、Prefab、模拟与文档恢复。

Debug 与 Release 合计四百三十个测试项通过。
GPU 回归保留中断与失败后的补跑证据。
五个移动项目包在 NVIDIA 和 Intel 上通过验证。
安装包包含右键漂浮说明及阶段验收记录。

正式性能测量仍有工作集增长超限。
原始失败、驻留页诊断及权限错误均保留。
实体键鼠与音效听感待复核，P5 保持 Active。

详细结果见[P5 验收](docs/acceptance/p5/2026-10-10.md)。

""";t=t.replace('# Changelog\n\n','# Changelog\n\n'+entry,1);p.write_text(t,encoding='utf-8')
print('Actual outcomes recorded; failed and manual gates retained')
