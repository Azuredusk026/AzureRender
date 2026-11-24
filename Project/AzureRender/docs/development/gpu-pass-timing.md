# GPU 通道计时

## 启用与输出

Player 的 `--gpu-pass-timing` 启用渲染图通道计时。
该选项同时启用原有帧计时。
通过 `--gpu-timing-output` 指定项目内的报告路径。
普通计时入口采用原有四个时间戳。

```powershell
AzurePlayer.exe --project scene.azureproject --gpu-pass-timing --gpu-timing-output D:/Projects/FYP/Project/AzureRender/build/runs/task/gpu.json
```

报告中的 `gpuPassTimings` 保存各通道的汇总。
字段包含名称、样本数量及均值、最小值和最大值。
同目录的 `gpu.passes.jsonl` 按行保存退役帧结果。
缺少可用结果的通道以 `available=false` 表示。

## 所有权与预算

宿主持有时间戳池与帧槽名称表。
每帧按图的执行顺序记录前六十四个通道。
每个通道使用两个时间戳，汇总至多保留一百二十八个名称。
重用帧槽前，按所属提交栅栏读取结果。

渲染图的可选边界回调在主命令缓冲执行。
开始边界位于资源屏障前，结束边界位于通道结束操作后。
图形、计算与主机命令使用相同契约。
并行录制的次级命令也由同一主命令边界包围。

## 数据解释

通道两端采用完成阶段的时间戳。
查询值按设备有效位宽和时间戳周期换算。
结果包含资源等待、命令间隙及管线排空影响。
跨通道重叠及驱动实现会影响归因精度。

开启分段计时会改变执行开销。
该入口用于确定热点和优化范围。
正式帧预算与候选净收益使用所属普通配置比较。
一次诊断的总进程时间还包含内容与管线准备。

实现依据为 [Vulkan 时间戳示例](https://docs.vulkan.org/samples/latest/samples/api/timestamp_queries/README.html)。
命令契约见 [vkCmdWriteTimestamp](https://docs.vulkan.org/refpages/latest/refpages/source/vkCmdWriteTimestamp.html)。
