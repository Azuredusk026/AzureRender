# AI 协作入口

本仓库的开发规则统一维护于 [AGENT.md](AGENT.md)。开始分析、实现或修改文档前，读取该文件。引擎定位、模块边界和 AI 协作约束适用于整个仓库。

## 自动执行

用户要求持续推进、继续自动执行或完整实施计划时，进入[自主实施模式](AGENT.md#自主实施模式)。
阶段验收和提交写入进度，随即执行计划内下一项。
单项用尽重试或时间预算后，登记阻塞并推进独立任务。
收尾前逐项核对剩余工作，确认完成或记录全部待解除的硬依赖。

调研入口为 [gkNextEngine 架构借鉴评估](Project/AzureRender/docs/research/2026-10-06-gknextengine.md)。正式任务见[引擎实施计划](Project/AzureRender/docs/plans/2026-10-06-engine-evolution.md)。执行顺序和状态见[开发总计划](Project/AzureRender/docs/plans/azure-engine-plan.md)。
