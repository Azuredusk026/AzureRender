# Pixel 6 正式采集续跑

用户暂停了设备采集。当前检查点包含区组 1 至 30 的 90 次独立运行和 90,000 个测量帧。
恢复入口为区组 31，使用 Pixel 6、2560 × 1440 和高复杂度，路径顺序为子通道、传统多遍、DRLR。
区组 31 至 40 共剩 30 次运行。冻结计划的 SHA-256 为 `140dfd0ced619d03b4d00ceccb9b9b67afe2961c21d8e8083e9ff98d2fa1c827`。

## 本地复核

以下命令在 PowerShell 中执行。检查点审计读取原始证据，另存两份统计流程验证表。

```powershell
$engine = 'D:/Projects/FYP/Project/AzureRender'
$build = Join-Path $engine 'build'
$run = Join-Path $build 'runs/20261112-215615-thesis-pixel6-experiment-b05cd7'
$py = Join-Path $build 'reinstall-environment/venv/Scripts/python.exe'
$audit = 'D:/Projects/FYP-worktrees/thesis-paths/Project/AzureRender/experiments/thesis/tools/audit_experiment.py'
& $py -B $audit $run --checkpoint --output (Join-Path $run 'analysis-checkpoint-2026-11-16')
```

复核结果为 `paused-checkpoint: verified 90 runs, 30/40 blocks, 90000 measured frames, 0 rejected blocks`。
受管目录持有桌面和 Android 两个原始构建输入的有效引用。

## 采集条件

1. 用户允许恢复后，连接原 Pixel 6 并确认 ADB 授权。用 `adb devices -l` 取得实际序列号，填入 `$serial`。
2. 核对设备代号 `oriole`、系统构建 `CP2A.260705.006`、Mali-G78 的原生 API 和驱动版本值 `4210993`、`226500608`。原生资格记录的 DRLR 功能位为真，显式设备创建为 `VK_SUCCESS`。用 `adb -s $serial shell cmd gpu vkjson` 复查驱动，按原生资格记录处理任何差异。
3. 核对冻结清单中的供电状态与热状态门槛，并检查手机的原基准与七份着色器仍在 `/data/local/tmp/az-thesis-20261112-b05cd7`。复用这批文件。采集入口再次核对设备构建、桌面二进制和两端着色器摘要，并在每次运行边界检查环境。
4. 核对检查点审计退出码为 0，使用共享调度提交一个正式测量批次。将队列命令和采集脚本放在同一正式窗口执行。

```powershell
& $py -B "$engine/tools/artifact_budget.py" --run $run --planned-bytes 8388608
if ($LASTEXITCODE -ne 0) { throw '受管任务预算检查失败' }
if ([string]::IsNullOrWhiteSpace($serial)) { throw '需要原 Pixel 6 的 ADB 序列号' }
$owner = '01a1c099-cf1e-7dd1-9229-d37bfb9bc82c'
$queue = 'D:/Projects/FYP-coordination/tools/dispatch.py'
& $py -B -X utf8 $queue run --build $build --owner $owner --task thesis-pixel6-formal-resume `
    --run (Split-Path $run -Leaf) --kind formal --estimate 360 --wait-timeout 7200 --timeout 4200 `
    --command $py -u -B (Join-Path $run 'experiment.py') formal $serial
if ($LASTEXITCODE -ne 0) { throw '采集入口返回失败，检查 process.log 和 formal-index.json' }
```

正式入口从 `formal-index.json` 的完整已接受区组继续。第 31 组接续前，原始索引状态为 `running`，暂停状态和续跑位置记录于 `pause-checkpoint.json`。
采集脚本的字节摘要受冻结清单约束。供电变化、摘要不匹配或设备身份变化时，按日志保留现场并核对原因。
如采集中断，先核对下一组是否出现尚未入索引的压缩文件，保存该次中断证据并建立可核对的恢复位置。

## 正式验收

完整索引应标记 `passed`，且包含 40 个区组及 120 次独立运行。每次预热 300 帧并记录 1,000 帧。

```powershell
& $py -B $audit $run --output (Join-Path $run 'analysis-formal')
```

记录续跑日期和中断边界，随正式结果说明两个采集窗口及各次运行的环境边界记录。
复核全部运行和 24 个试运行条件后生成正式图表与论文结果。完成证据归档后按项目维护流程封存任务。
手机文件清理沿用该任务的 `cleanup` 入口及共享队列，范围为该任务创建的手机目录。
