# 正式功率条件核定

日期：2026-11-10。
本机驱动版本为 592.82。
RTX 4060 Laptop GPU 默认功率上限为 80W。
当前请求值及强制值随观察变化，正式条件继续待满足。

## 实际控制

共享资源互斥窗口内执行 `nvidia-smi -i 0 -pl 80`。
驱动返回 `Insufficient Permissions`，设置未成功。
随后查询的请求值及强制值均为 88.88W。
驱动公布的设置范围为 5W 至 140W。

命令接口见 [NVIDIA 管理工具说明](https://docs.nvidia.com/deploy/nvidia-smi/index.html)。
本次使用现有进程权限，未启动提权流程。
正式固定功率需要有权限的设备控制入口。
权限或设备条件变化后，再核对实际强制值。

## 验收范围

本单元新增渲染采样次数为零。
环境阻塞、短时诊断与正式性能结果分别登记。
CPU 八毫秒与 GPU 16.6 毫秒标准保持有效。
可独立推进的程序和成本诊断继续执行。

[结果清单](evidence/2026-11-10/gpu-power-limit-qualification/results.json)
与[验证归档](evidence/2026-11-10/gpu-power-limit-qualification/validation.zip)
保存设置前后查询、命令及驱动原文。
