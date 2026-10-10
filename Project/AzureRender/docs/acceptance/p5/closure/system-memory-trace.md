# 系统内存追踪操作单

> 文档类型：待执行诊断流程
> 日期：2026-10-11
> 状态：当前进程权限不足，系统追踪待执行

## 输入与目的

使用最终 Release 来源清单中的 Player。
诊断覆盖申请、提交、取消提交和区域释放。
驻留记录用于关联实际工作集变化。
调用栈用于识别引擎、运行库与驱动的归属。

本机 WPR 提供两项所需追踪配置。
`VirtualAllocation` 记录虚拟分配事件。
`ResidentSet` 记录驻留集合。
当前进程的管理员状态查询为否。

普通启动、页面分类与调试追踪均保留。
调试器并发返回对照存在漏采。
进程内原型通过四十个已知调用配对。
正式失败的完整因果仍需独立复核。

局部结果见[进程内诊断](native-memory-trace.md)。
系统追踪使用对应正式负载的项目与输入。
记录输入文件和项目内容的哈希。
正式性能和系统追踪分别报告。

## 采集步骤

在管理员 PowerShell 中打开工程目录。
先核验最终 `build-provenance.json`。
为本次追踪选择尚未存在的输出目录。
以下命令使用独立的追踪会话名。

```powershell
python tools/build_provenance.py verify --build-dir build/ninja-msvc-release --config Release
New-Item -ItemType Directory -Path build/p5-system-trace
Copy-Item -LiteralPath build/p5-p6-closure-2026-10-10/final-formal-performance/exploration-1.input.json -Destination build/p5-system-trace/input.json
wpr -start VirtualAllocation -start ResidentSet -filemode -instancename AzureP5Memory
```

确认启动命令退出码为零后运行诊断负载。
保留控制台输出和测量文件。
程序正常退出后保存系统事件文件。

```powershell
./build/ninja-msvc-release/AzurePlayer.exe --project assets_public/exploration/project.azureproject --game-actions build/p5-system-trace/input.json --width 1920 --height 1080 --fixed-frame-step --smoke-frames 2100 --gpu-timing --gpu-timing-output build/p5-system-trace/timing.json --runtime-report build/p5-system-trace/runtime.json
wpr -stop build/p5-system-trace/memory.etl -instancename AzureP5Memory
```

程序失败时仍执行同一会话的保存命令。
启动失败时保留错误，不操作其他追踪会话。
命令参数已按本机 WPR 帮助核对。
实际采集尚未执行，结果以原始文件为准。

## 归因与门禁

在系统性能分析器中按 Player 进程筛选。
登记分配地址、大小、调用栈和成功状态。
关联提交、取消提交、释放与驻留时间。
按加载、运行、重开、切关与退出比较快照。

分析同时登记事件丢失与采样覆盖范围。
驱动后台线程和显式进程句柄需单独检查。
明确未归因大小，提出可重复的最小实验。
修复需通过原性能预算及生命周期回归。

证据包括事件文件、源码和程序指纹。
另保存 GPU、驱动、温度和测试环境。
原始失败见[内存调查](memory-investigation.md)。
