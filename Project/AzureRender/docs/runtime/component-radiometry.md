# 渲染分量测量

Player 与编辑宿主通过 `--qa-isolation` 选择分量。
辐射分量可通过 `--qa-exposure` 设置诊断曝光。
该参数为 `[-8,8]` 内的有限 EV，缺省为零。
完整画面的曝光通过场景及公共渲染设置配置。

```powershell
AzurePlayer.exe --project scene.azureproject `
  --qa-camera scene --qa-light scene `
  --qa-isolation hair-kk --qa-exposure -4 `
  --capture-dir frames --capture-frames 32 --fixed-frame-step
```

## 支持范围

支持基础颜色、KK、边缘光、镜面与自发光。
环境、直接漫反射、直接镜面、反射及覆盖层也支持。
该参数要求显式选择辐射分量。
分类、法线、深度及效果隔离使用其规定的显示规则。

分量输出使用中性色调及关闭的色调映射。
诊断曝光在历史合成后、输出编码前应用。
曝光因子为 `2^EV`，当前 Alpha 保持输入语义。
KK 高光的既定显示倍率另按分量规则计算。

`hair-kk`、`rim`、`specular` 的显示倍率为三。
例如 KK 在负四 EV 时的总倍率为 `3/16`。
定量比较须固定曝光并确认测量区域未发生截断。
参考与候选使用相同显示倍率及曝光。

## 诊断记录

采集清单中的 `qaExposureEv` 记录请求值。
`gradeSettings.exposureEv` 记录实际生效值。
实际曝光参与现有采集状态指纹。
场景文档保留作者定义的调色和曝光参数。

分量测量消费现有调色通道与纹理资源。
材质表、相机缓冲及渲染器 API 保持既有布局。
缺省与显式零 EV 的输出需逐字节一致。
正负曝光通过线性转换及独立量化区间检查。

材质画质门禁见[抗锯齿计划](../plans/2026-10-22-spatial-antialiasing.md)。
