# 宿主帧调度

> 状态：候选实现，运行对照验收中。
> 范围：Windows 编辑器与独立 Player。

## 设置与职责

`runtime.maxFrameRate` 定义宿主每秒帧数上限。
设置类型为整数，范围为零至一千。
零表示无限制，候选默认值为六十。
设置支持用户、项目、命令行和会话来源。

帧边界应用设置后，主循环读取有效值。
一帧包含事件处理、工具调度和实际渲染。
剩余帧预算由 `framePacingWaitSeconds()` 计算。
宿主使用单调时钟执行剩余等待。

函数位于 Foundation，保持独立于玩法和渲染。
已超出帧预算时返回零等待。
等待期间保持本帧工作与模拟结果。
后续帧按正常顺序执行输入和关闭检查。

## 配置入口

用户可以在设置面板中查询和修改该字段。
自动入口使用相同设置描述与版本约束。
命令行通过 `--set` 提供类型化覆盖。
设置重置恢复较低来源的有效值。

`runtime.vsync` 是启动专用布尔设置。
候选默认值为真，对应 FIFO 同步呈现。
设置为假时优先选择设备支持的 MAILBOX。
设备仅支持 FIFO 时，使用该模式。
修改设置后启动新的宿主会话。

```powershell
AzurePlayer.exe --project game/project.azureproject --set runtime.maxFrameRate=60
AzureRender.exe --editor-project game/project.azureproject --set runtime.maxFrameRate=0
AzurePlayer.exe --project game/project.azureproject --set runtime.vsync=false
```

`--capture-fps` 定义固定模拟及视频的帧率。
`runtime.maxFrameRate` 定义宿主的真实调度上限。
固定模拟仍使用声明的固定步长。
实时模拟仍依据实际帧间时间执行。

## 诊断与测量

GPU 时间记录包含 `framePacing` 对象。
`maximumRate` 是记录时的有效设置。
`waitFrames` 是执行等待的帧数。
`waitMilliseconds` 是实际等待累计时间。
`vsync` 记录有效同步设置。
`presentMode` 记录实际交换链呈现模式。

等待在帧工作计量之外单独记录。
正式性能结果保留帧率设置和原始观察。
渲染画质、分辨率及采样数量按各验收规定执行。
持续环境记录同时核验功耗、频率和热降频。

该上限不能保证设备达到目标帧率。
实际性能仍取决于工作负载和设备。
候选对照见[实施计划](../plans/2026-10-11-frame-pacing.md)。
实体操作与反馈在 P8 集中复核。
