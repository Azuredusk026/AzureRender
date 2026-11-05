# 完整原生场景显示

本页提供当前联合画面、可玩项目及冻结交付入口。
工作目录为 `D:/Projects/FYP/Project/AzureRender`。
运行前完成[环境恢复](../plans/2026-10-30-development-recovery.md)。
任务顺序见[开发总计划](../plans/azure-engine-plan.md)。

完整联合画质保持 `failed-user-review` 状态。
功能验证、局部数值与用户外观复核分别登记。
任务预算及验证范围见[协作与验收](../development-workflow.md)。

## 联合场景入口

当前项目使用完整物流场景、十四个角色材质槽及十段动画。
皮肤基础颜色因子为 `[0.98,0.93,0.89,1]`。
场景使用三十三层反射图集及原生辐照体。
待机角色与中央物流采用项目声明的动画时钟。

```powershell
$run = Get-Content build/current-character-reference-run.txt
& build/ninja-msvc-release/AzurePlayer.exe `
  --project "$run/qualified-lit-surfaces-joint-v4/joint-materials.azureproject" `
  --width 2560 --height 1440 --qa-camera scene --qa-light scene
```

该入口消费当前构建的 Player 及着色程序。
两配置的四帧与资源释放见[快照验证](../acceptance/nr/outline-snapshot-order.md)。
192 帧动画及可玩路线见[联合记录](../acceptance/nr/evidence/2026-11-05/lit-surfaces-motion/results.json)。

## 固定序列采集

运行目录由[产物工具](../development/artifact-retention.md)管理。
以下命令采用固定步长，每秒采样二十四帧。
输出路径用于本次独立采集。

```powershell
$run = Get-Content build/current-character-reference-run.txt
$captureRun = & build/reinstall-environment/venv/Scripts/python.exe `
  tools/artifact_cleanup.py --new-run joint-visual-capture
& build/ninja-msvc-release/AzurePlayer.exe `
  --project "$run/qualified-lit-surfaces-joint-v4/joint-materials.azureproject" `
  --width 2560 --height 1440 `
  --capture-frames 4 --capture-fps 24 --fixed-frame-step `
  --capture-dir "$captureRun/frames" --qa-camera scene --qa-light scene
```

输出包含 PNG、设备信息及实际采集清单。
采样帧率描述固定时间取样，持续性能单独测量。
复现记录包含程序、着色目录、关卡及依赖摘要。
同时记录机位、姿态、灯组、曝光、反射路径及时间。

## 可玩项目

开发入口使用当前构建程序及可编辑的项目资源。

```powershell
$run = Get-Content build/current-character-reference-run.txt
& build/ninja-msvc-release/AzurePlayer.exe `
  --project "$run/qualified-lit-surfaces-playable/playable.azureproject" `
  --width 1920 --height 1080 --qa-camera scene --qa-light scene
```

单击游戏窗口获取操控焦点。
WASD 移动，Shift 冲刺，移动鼠标转动相机。
R 重开当前关卡，Esc 释放鼠标和焦点。
首次启动和重开需要准备完整场景资源。

## 冻结交付入口

在当前运行目录中打开：
`lit-surfaces-delivery/独立游戏 中文 空格/完整联合场景显示验证.exe`。
该包使用固定版本的 Player、项目及着色程序。
操作说明位于包内 `GAME-GUIDE.md`。

交付包含 2,718 个文件，默认分辨率为 1920×1080。
保存、目录移动及隔离运行见[交付核验](../acceptance/nr/lit-surfaces-delivery.md)。
实际包与程序摘要见[交付清单](../acceptance/nr/evidence/2026-11-05/lit-surfaces-delivery/results.json)。
包内版本与当前构建分别记录来源及验收范围。

## 场景与角色参考

独立场景采用备份机位及其来源资源。
启动和严格对照见[完整场景复核](../acceptance/recovery/full-scene-visual.md)。
反射细节、间接照明及固定箱体见[场景分项](../acceptance/nr/scene-material-quality-diagnosis.md)。
当前辐照体的实际贡献具有同机位启停对照。

正面角色具有独立机位、灯组、曝光及捕获姿态。
输入及运行方法见[角色参考](../acceptance/recovery/character-reference.md)。
完整角色、皮肤及头发的固定输入见[阶段计划](../plans/2026-11-03-character-reference-stages.md)。
各参考按自身输入条件解释结果。
