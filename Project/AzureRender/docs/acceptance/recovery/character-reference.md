# 角色参考入口与渲染差异

日期：2026-10-31。
范围为用户提供的双角色正面参考图。
状态：原始入口已复现，严格像素兼容继续验收。
联合场景的完整角色外观仍待验收。

## 参考来源

原始图片为 `beauty-reference-comparison.png`。
来源批次为 `20261024-074605-native-material-aa-reference-9eb92d`。
生成脚本、输入项目和采集命令均保留在备份中。
附件采用 JPEG，识别依据尺寸、构图及图像对照。

左图为 5120×2880 的单帧参考。
脚本在线性显示颜色域执行四倍面积平均。
右图为 1280×720 时间抗锯齿的第三十二帧。
两幅头部区域分别缩放为 290×310 后拼接。

![历史角色参考](evidence/2026-10-31/character-reference/historical-reference.png)

## 入口差异

同一仓库包含多个角色候选和场景入口。
复现由程序、资产、场景、采样及显示变换共同确定。
每次对照须锁定实际使用的入口与内容摘要。
代码仓库名称本身不能完整描述渲染输入。

| 项目 | 用户参考入口 | 完整环境联合场景 |
| --- | --- | --- |
| 角色模型 | `outline-converted.glb`，捕获姿态 | `hero-restored-inputs-v7.glb`，九段动画 |
| 材质绑定 | 捕获材质及具名覆盖 | 十三份完整材质文档 |
| 抗锯齿 | 时间抗锯齿，累计三十二帧 | 模式一，边缘抗锯齿 |
| 曝光 | −2 EV | 0 EV |
| 色调映射 | 关闭 | 开启 |
| 调色查找表 | 空 | `gi:/captured-3534.azlut` |
| 灯光 | 十盏，捕获角色条件 | 六盏，完整环境条件 |
| 风格化环境来源 | 显式颜色及捕获参数 | 场景辐照体积 |
| 主光可见量覆盖 | 1 | 默认 0 |

这些差异改变颜色、入射能量及高光形状。
抗锯齿和采样历史还影响发丝与轮廓的连续性。
联合画面的既有检查范围为内容装配及运行。
其完整角色外观需要按参考条件继续校正。

## 眼睛材质

用户参考的虹膜启用 `eye-radial-surface`。
专用表面消费视差、Matcap、高光及散射颜色。
完整环境联合项目也声明该专用表面。
它在联合场景中消费另一套照明和显示变换。

`hair-receiver.glb` 属于独立的头发接收测试。
该测试的虹膜使用基础漫反射色带。
用户参考图的眼睛验收使用原始双图项目。
该项目持有完整材质及原始采集条件。

## 当前机器复现

原始输入按字节复制到独立目录。
两组采集均使用当前 Release Player。
实际输出包含三十二张时间序列和一张高分辨率图。
两次进程正常退出，卸载资源计数归零。

![当前机器重新渲染](evidence/2026-10-31/character-reference/current-reference.png)

| 原始 PNG 对照 | 相同像素比例 | 平均绝对通道差 | 最大通道差 |
| --- | --- | --- | --- |
| 高分辨率参考 | 99.880710% | 0.000589 | 16 |
| 时间抗锯齿末帧 | 96.353950% | 0.059854 | 234 |

通道差的取值范围为零至二百五十五。
时间抗锯齿头部区域的平均通道差约为 0.5725。
整区及严格 PNG 门槛仍保持未通过状态。
时间边缘、采样与程序版本需要继续分项核对。

历史 Player 摘要以 `e8b98db469` 开头。
当前 Player 摘要以 `8bac912e35` 开头。
自动开发已更新程序及着色产物，版本按清单记录。
残余像素差异的代码与设备归因仍待确认。

## 重现命令

工作目录为 `D:/Projects/FYP/Project/AzureRender`。
以下命令保留原始时间抗锯齿采集条件。
输出目录须为空。

```powershell
$characterRun = & build/reinstall-environment/venv/Scripts/python.exe `
  tools/artifact_cleanup.py --new-run character-reference-capture
& build/ninja-msvc-release/AzurePlayer.exe `
  --project build/runs/20261031-135416-character-portrait-recovery-0773ca/original-reference/project/temporal.azureproject `
  --width 1280 --height 720 `
  --capture-frames 32 --fixed-frame-step `
  --capture-dir "$characterRun/frames" `
  --qa-camera scene --qa-light scene --qa-isolation beauty `
  --set render.antiAliasing=3
```

项目及完整依赖目录带有保留标记。
原始模型、材质、纹理和场景均保留内容摘要。
图片、完整序列及实际命令见[结构化记录](evidence/2026-10-31/character-reference/summary.json)。
照明单参数候选的状态见[配置计划](../../plans/2026-10-31-native-character-visibility.md)。

## 接续验证

眼睛、头发和服装使用同一参考入口分别核对。
完整环境组合需要校正角色照明与颜色传输。
场景自身的阴影、反射和物流内容继续独立回归。
原生完整外观、动态稳定性及性能标准保持有效。
