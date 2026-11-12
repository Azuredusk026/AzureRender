# 正式功率条件核定

日期：2026-11-10。
当前驱动的功率设置入口报告设备不支持。
正式固定 80W 条件继续待满足。
设备控制与代码验收分别推进。

## 实际检查

本机普通进程执行 `nvidia-smi -i 0 -pl 80`。
驱动返回 `Insufficient Permissions`。
用户提供的管理员 PowerShell 执行结果如下。

```text
Changing power management limit is not supported for GPU: 00000000:01:00.0.
Treating as warning and moving on.
All done.
```

管理员结果记录为设置未受支持。
用户未提供退出码，该字段保存为空值。
后续核对需要具备相应能力的驱动或厂商控制入口。
实际强制值继续按驱动查询记录。

命令接口见 [NVIDIA 管理工具说明](https://docs.nvidia.com/deploy/nvidia-smi/index.html)。
初次设备查询的驱动版本为 592.82。
该次默认上限为 80W，公开范围为 5W 至 140W。
完整查询保存在所属归档。

## 厂商控制面板

本机为 HP OMEN 16-wf0xxx，已安装 OMEN Gaming Hub 1101.2609.11.0。
2026-11-12 打开应用时，应用报告版本过期并要求更新。
选择稍后更新后，界面显示更新失败，设备性能控制页仍被弹窗阻挡。
本次检查未更改设备设置。

同日驱动查询的请求功率上限为不可用，单次读取的实际强制上限为 79.83 W，默认上限为 80 W。
固定上限须通过实际采样期间的连续记录核定。
HP 文档将本机系列列在支持高级性能模式的范围内，并将性能模式的功率分配描述为动态调节。
参见 [HP 机型功能表](https://support.hp.com/nz-en/document/ish_9237683-9237735-16)和 [HP 性能模式说明](https://support.hp.com/ee-en/document/ish_3912817-3737596-16)。

## 验收范围

本单元新增渲染采样次数为零。
短时诊断与正式性能分别登记。
CPU 八毫秒与 GPU 16.6 毫秒标准保持有效。
可独立推进的代码与画质验收继续执行。

[结果清单](evidence/2026-11-10/gpu-power-limit-qualification/results.json)
与[验证归档](evidence/2026-11-10/gpu-power-limit-qualification/validation.zip)
保存本机查询及用户提供的管理员结果。
