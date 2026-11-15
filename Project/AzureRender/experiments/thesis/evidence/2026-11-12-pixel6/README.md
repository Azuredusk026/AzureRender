# 跨平台三路径采集记录

设备为 Pixel 6（Mali-G78）和 GeForce RTX 4060 Laptop GPU。
试运行原始文件及运行前后环境见 [pilot-index.json](pilot-index.json)。
Pixel 6 的实际功能位及设备创建结果见[原生资格记录](../pixel6-qualification/)。
两个程序及七个 SPIR-V 文件的摘要见 [formal-plan.json](formal-plan.json)。

## 正式采集

矩阵包含两个尺寸、两档复杂度和三条离屏路径。
每个条件运行五个独立进程，每次预热 300 帧并测量 1,000 帧。
每轮打乱八个设备与负载组的执行顺序。
同组的三路径相邻执行，五轮采用五种不同排列。
随机种子和逐组顺序保存在冻结清单。

采集关闭验证层、呈现与捕获。
Pixel 6 的运行边界检查 Android 系统热状态。
RTX 4060 的运行边界检查 NVIDIA 软件及硬件热降频标志。
供电状态与试运行核对，图像摘要与对应试运行条件核对。

## 复核

受管任务的 `formal-index.json` 记录冻结清单摘要、接受的运行及排除记录。
受管任务内的 `formal/*.json.gz` 解压后是基准输出的原始 JSON 字节。
原始字段 `status` 是基准程序的固定标签，正式采集资格由冻结清单和运行索引给出。
受管任务内的 `experiment.py` 保存当次冻结的采集脚本。
[暂停检查点](pause-checkpoint.json)记录 90 次已接受运行及第 31 区组的恢复位置。
复核命令、现有输入和正式采集步骤见[续跑记录](../../docs/resume.md)。

检查点统计使用当时已有的三次或四次独立运行，范围标为 `paused-checkpoint`。
条件表、运行方差及配对统计定义见[统计与附件模型](../../docs/analysis.md)。
