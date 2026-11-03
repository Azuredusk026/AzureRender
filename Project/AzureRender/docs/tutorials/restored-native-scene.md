# 完整原生场景显示

本页使用恢复后的完整物流场景。
工作目录为 `D:/Projects/FYP/Project/AzureRender`。

先完成[环境恢复](../plans/2026-10-30-development-recovery.md)。
画面与范围见[完整场景复核](../acceptance/recovery/full-scene-visual.md)。
联合角色画质验收未通过，诊断状态见[输入核验](../acceptance/nr/joint-visual-input-isolation.md)。

## 联合场景验收入口

当前联合入口包含完整物流场景和待机角色。角色按原始比例放置，镜头距离为 3.82 米。以每秒 24 帧采集时，八秒内共有 192 帧。

```powershell
$run = Get-Content build/current-character-reference-run.txt
& build/ninja-msvc-release/AzurePlayer.exe `
  --project "$run/qualified-hair-facing-joint/joint-materials.azureproject" `
  --width 2560 --height 1440 --qa-camera scene --qa-light scene
```

程序、动画帧及重开校验值见[联合动画记录](../acceptance/nr/evidence/2026-11-04/translucent-hair/motion.json)。以下命令用于单独检查环境。

## 可玩交付候选

在当前运行目录的 `playable-delivery-current/独立游戏 中文 空格` 中双击 `完整联合场景显示验证.exe`。操作说明见包内 `GAME-GUIDE.md`。该候选使用包内固定版本的场景和播放器，功能范围及校验值见[独立发布核验](../acceptance/nr/evidence/2026-11-04/joint-playable-delivery/results.json)。

## 独立场景启动

在工程目录运行以下命令。

```powershell
& build/ninja-msvc-release/AzurePlayer.exe `
  --project build/runs/20261029-045743-snapshot-shading-budget-8ee0b1/full-candidate/enclosure-sampler.azureproject `
  --width 1920 --height 1080 `
  --qa-camera scene --qa-light scene --qa-isolation beauty
```

项目绑定原生调色查找表及完整物流图层。
相机采用捕获场景中声明的机位。
默认配置使用模式一及线性时间反射。
首次启动需要扫描资源并准备图形管线。

## 固定序列采集

任务目录通过产物工具生成。
以下命令采集与归档对应的十二帧序列。

```powershell
$sceneRun = & build/reinstall-environment/venv/Scripts/python.exe `
  tools/artifact_cleanup.py --new-run full-scene-capture
& build/ninja-msvc-release/AzurePlayer.exe `
  --project build/runs/20261029-045743-snapshot-shading-budget-8ee0b1/full-candidate/enclosure-sampler.azureproject `
  --width 2560 --height 1440 `
  --capture-frames 12 --capture-fps 60 --fixed-frame-step `
  --capture-dir "$sceneRun/frames" `
  --qa-camera scene --qa-light scene --qa-isolation beauty
```

输出目录包含 PNG、采集清单及设备信息。
序列帧率用于固定时间取样，持续性能另行测量。
归档前保存命令、日志和输入指纹。
任务完成后按[产物规则](../development/artifact-retention.md)标记目录。

## 完整场景操控

独立游戏的入口与范围见[保存迁移与独立运行](../acceptance/recovery/full-scene-delivery.md)。
该目录包含实际原生入口、Player 及完整项目资源。
开发态项目使用以下命令启动。

```powershell
& build/ninja-msvc-release/AzurePlayer.exe `
  --project build/runs/20261031-183314-full-scene-playable-validation-d5257e/project/playable.azureproject `
  --width 1920 --height 1080 `
  --qa-camera scene --qa-light scene --qa-isolation beauty
```

单击游戏窗口获取操控焦点。
WASD 移动，Shift 冲刺，移动鼠标转动相机。
R 重开当前关卡，Esc 释放鼠标和焦点。
同关卡重开包含较长的备用资源准备。

该入口使用完整环境与 V8 动态角色。
独立回放、碰撞及画面范围见[操控验收](../acceptance/recovery/full-scene-playable.md)。
保存、移动和独立发布按交付接续验收。

控制组件及原生片段的来源见[角色项目](../acceptance/nr/main-character-integration.md)。
该来源项目具有独立的相机、照明和内容组合。

## 完整环境与角色

联合项目的画面与范围见[组合验证](../acceptance/recovery/full-scene-character.md)。
该项目保留完整环境的挂载、原机位及调色配置。
角色采用恢复主坐标的动态资产及原待机动画。
项目目录带有保留标记，挂载资源与来源项目共同维护。

```powershell
& build/ninja-msvc-release/AzurePlayer.exe `
  --project build/runs/20261031-172112-hair-source-inputs-14d199/project/full-restored.azureproject `
  --width 1920 --height 1080 `
  --qa-camera scene --qa-light scene --qa-isolation beauty
```

固定序列采用 `--capture-frames 12 --capture-fps 60`。
同时传入 `--fixed-frame-step` 及独立的 `--capture-dir`。
对照项目为同目录的 `hidden-restored.azureproject`。
头发坐标及严格隐藏对照见[坐标验收](../acceptance/nr/hair-source-uv.md)。

严格环境像素兼容按组合验证页记录实际状态。

## 正面角色参考

用户提供的正面双图具有独立项目和捕获姿态。
材质、曝光及时间采样按[角色参考](../acceptance/recovery/character-reference.md)执行。
该入口用于核对眼睛、头发和服装的实际外观。
完整环境组合继续使用本页的联合场景入口。

## 动态角色正面诊断

完整环境中的角色近景见[照明联合验证](../acceptance/nr/character-illumination-integration.md)。
诊断保持动态模型、原待机动画及完整材质。
项目的输入资产均位于自身挂载目录中。

```powershell
& build/ninja-msvc-release/AzurePlayer.exe `
  --project build/runs/20261031-172112-hair-source-inputs-14d199/project/near-restored.azureproject `
  --width 1536 --height 1536 `
  --qa-camera scene --qa-light scene --qa-isolation beauty
```

该入口采用关闭抗锯齿的固定诊断机位。
底色诊断使用 `--qa-isolation albedo`。
照明候选及输入条件见[联合验证](../acceptance/nr/character-illumination-integration.md)。
整体外观及时间稳定性仍按正式门禁验收。
