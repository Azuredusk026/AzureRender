# 材质程序离线优化

十一种捕获材质程序使用 SPIRV-Tools 性能优化序列。
原程序共 258120 条指令，候选为 193475 条。
合计规模减少约 25.04%，实际 GPU 收益待测。
专用常量编号保留，十一项合法性校验通过。

四处事件的九份附件逐字节一致。
比较覆盖中间浮点附件及最终显示附件。
进程在退出阶段触发 240 秒超时。
完整运行资格和原生四帧资格继续待验。

本单元使用电池供电，未采集 GPU 性能计数器。
候选保留于独立归档。
两配置来源核验通过，各 663 项生产程序字节一致。
交流供电恢复后，先定位退出阶段，再评估收益资格。

[结构化结果](evidence/2026-11-07/material-spirv-optimization/results.json)和
[候选归档](evidence/2026-11-07/material-spirv-optimization/candidate.zip)
保存命令、摘要、程序及完整附件比较。
工具依据见 [SPIRV-Tools](https://github.com/KhronosGroup/SPIRV-Tools)。
