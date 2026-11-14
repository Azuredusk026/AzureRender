# 主线 CPU 注册回归

2026-11-14，Release 与 Debug 各通过 333 项 CPU 注册用例。两种配置均无失败或跳过。完整 GPU 回归使用同批构建产物继续执行。

[主线 GPU 定向复验](current-registered-gpu-recoveries.md)记录同批产物上三项已通过的复验。

| 配置 | CPU 通过 | GPU 注册 | 注册总数 |
| --- | ---: | ---: | ---: |
| Release | 333 | 274 | 607 |
| Debug | 333 | 272 | 605 |

每种配置的 12 组收据共同覆盖全部 CPU 用例。归档核对了用例名称、原注册清单及构建产物摘要。已恢复的着色器项目见[着色器注册回归](frozen-public-visual.md#着色器注册回归)。

调度收据保存 CTest 的退出码、失败和跳过项。含失败或跳过的分组进入复核。隔离检查使用实际 CTest，覆盖通过、失败与跳过三种结果。

[结果清单](evidence/2026-11-14/current-registered-cpu/results.json)及[验证归档](evidence/2026-11-14/current-registered-cpu/verification.zip)保存两种配置的分组结果、输入与产物指纹及调度检查。
