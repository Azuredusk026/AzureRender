# 性能采样供电预检

共享环境观察器保存 Windows 系统供电状态。
实时测量要求交流电明确接通。
电池及未知状态在空闲等待前保存诊断并拒绝。
周期观察中的供电变化进入该轮资格判断。

三项新增失败用例覆盖即时拒绝及运行中断电。
环境二十三项和性能预算十二项检查通过。
本机只读查询报告交流供电、电量 80% 及 75W 限制。
该观察只证明当时状态，实际测量另行核对环境。

两配置来源核验及产物字节核对通过。
各配置的运行程序与 663 项着色程序均一致。
本单元未启动 GPU 工作负载。
历史记录继续按已有字段表达其观察范围。

[结构化结果](evidence/2026-11-07/performance-power-preflight/results.json)和
[检查归档](evidence/2026-11-07/performance-power-preflight/checks.zip)
保存失败用例、通过记录、系统观察及构建命令。
字段定义依据 [Windows 电源状态](https://learn.microsoft.com/en-us/windows/win32/api/winbase/ns-winbase-system_power_status)。
