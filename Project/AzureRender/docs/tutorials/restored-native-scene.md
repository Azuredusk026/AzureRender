# 完整原生场景显示

本页使用恢复后的完整物流场景。
工作目录为 `D:/Projects/FYP/Project/AzureRender`。
先完成[环境恢复](../plans/2026-10-30-development-recovery.md)。
画面与范围见[完整场景复核](../acceptance/recovery/full-scene-visual.md)。

## 启动

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

## 角色联合入口

角色材质及可玩操作使用[角色项目](../acceptance/nr/main-character-integration.md)。
该入口具有独立的相机、照明和内容组合。
联合制作同时回归完整物流场景和角色材质。
通用渲染能力按各自项目配置装配。

## 完整环境与角色

联合项目的画面与范围见[组合验证](../acceptance/recovery/full-scene-character.md)。
该项目保留完整环境的挂载、原机位及调色配置。
角色采用固定世界位置及原待机动画。
项目目录带有保留标记，挂载资源与来源项目共同维护。

```powershell
& build/ninja-msvc-release/AzurePlayer.exe `
  --project build/runs/20261031-105539-full-scene-character-composition-b37668/project/full-character-visible.azureproject `
  --width 1920 --height 1080 `
  --qa-camera scene --qa-light scene --qa-isolation beauty
```

固定序列采用 `--capture-frames 12 --capture-fps 60`。
同时传入 `--fixed-frame-step` 及独立的 `--capture-dir`。
对照项目为同目录的 `full-character-hidden.azureproject`。
严格环境像素兼容按组合验证页记录实际状态。

## 正面角色参考

用户提供的正面双图具有独立项目和捕获姿态。
材质、曝光及时间采样按[角色参考](../acceptance/recovery/character-reference.md)执行。
该入口用于核对眼睛、头发和服装的实际外观。
完整环境组合继续使用本页的联合场景入口。
