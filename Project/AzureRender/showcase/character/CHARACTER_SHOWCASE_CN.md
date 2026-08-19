# 角色场景展示

## 当前交付

技术展示由五个 16 秒段落组成，依次为最终渲染、原始模型（Albedo）、世界法线、阴影可见度和材质 ID 分区。每段使用同一固定相机、脚部中心转台和 `2π/16 rad/s` 角速度，从正面开始精确旋转一整圈。旧视频的描边、深度和仅剩黑底发丝的 Hair KK 段已移除。画面原生输出 1600×900，编码强制 SAR 1:1，不做非等比拉伸。

| 文件 | 内容 | SHA-256 |
| --- | --- | --- |
| `images/20260819-200656_角色加宽眉毛近景.png` | 2 倍纵向卡片几何的深红眉毛近景 | `E4128AB1C2E305EB9EF1588CFF69E9AB50D9B6E6F755166B8FD5147909A15439` |
| `images/20260819-200656_角色正面最终渲染.png` | 最终渲染正面与远景眉毛 | `D982820EEF6ABBBBBA0EE10A55F51EDB8DA4A7605A65E74E3E63A846F94DFF9F` |
| `images/20260819-200656_角色侧面最终渲染.png` | 四分之一圈后的侧面明暗 | `444EA17CB05A7513EED5DE6FB066FF6C8053335D21C1DB506CCADF34D7FD32C6` |
| `images/20260819-200656_角色原始模型.png` | Albedo 原始贴图与材质底色 | `3A9186461581654484AD68C1F0DC37A6B5C30D6569F23A03A1B512999D198065` |
| `images/20260819-200656_角色法线分布.png` | 世界空间法线 | `3F5A111AD87F801AFD62802A099BCF0509032A5D94E9D63FF00244811B5BEB59` |
| `images/20260819-200656_角色阴影分布.png` | 主光和 shadow-map 可见度 | `355B700E60039234F1F6A065FCB7A180C54B25B60C1905E37F1247F43E7A21B0` |
| `images/20260819-200656_角色材质分区.png` | Skin/Hair/Fabric/Overlay/Platform 分类 | `F4BD457A36580119634EAFE56B1F41E26BAEFD2BE91996E5976B6EC5C97F020B` |
| `video/20260819-200656_角色五模式整圈展示.mp4` | 1920 帧、80.00 秒、5 个完整转台段落 | `334206DFEE5BDBDDC8B40B742030E57BFD16B2FEC64567286998CFA478B7DE35` |

生成环境：Debug Validation、NVIDIA GeForce RTX 4060 Laptop GPU、Evening Sky 环境、Endfield Industrial Look。视频已完整解码 1920 帧，格式为 H.264 High、yuv420p/BT.709、1600×900、24 fps、SAR 1:1、DAR 16:9。

## 眉毛与头发数据

- `M_actor_laevat_brow_01` 使用 `brow-overlay` 专用 Unlit 通路：Face D RGB、0.95 不透明度，以及沿视线方向 `0.04679 m` 的顶点偏移；该数值对应原资产的 `4.679 cm`。
- 针对导出后贴近上眼睑的眉毛卡片，材质数据额外提供 `0.01 m` 局部上移；Face D 结果乘深红显示补偿，保证眉毛在 HDR 合成后仍与眼线分离且可见。
- 为增加全身正面画面的实际像素覆盖，眉毛卡片围绕导出中心 `1.2536046 m` 做 2 倍纵向几何放大；放大仅作用于 Brow primitive，不改变脸部或眼线。
- `T_actor_laevat_hair_01_D`：头发 Base Color。
- `T_actor_laevat_hair_01_HN`：独立 Hair Data；RG 参与基础发束法线，BA 驱动双层 Kajiya-Kay 高光方向。
- `T_actor_laevat_hair_01_P`：Hair Master 的 `_P` packed 材质数据；注入器同时支持 `_P` 与布料使用的 `T_RGBA_P`。

## 复现

五个段落均捕获 384 帧。把 `--qa-isolation` 依次设为 `beauty`、`albedo`、`world-normal`、`shadow-visibility` 和 `material-id`；`--portfolio` 与 isolation 组合时保留自动转台。

```powershell
.\build\ninja-debug\AzureRender.exe `
  --scene-type character `
  --asset .\assets_private\laevat_skinned\laevat_idle_material.glb `
  --portfolio --width 1600 --height 900 `
  --capture-dir .\build\character_beauty `
  --qa-isolation beauty `
  --capture-frames 384 --capture-fps 24
```

私有角色模型、派生 GLB、纹理、截图和视频仅供本机视觉检查，不得提交 Git、进入 CI/安装包或公开 portfolio。公共自动化回归继续使用 `assets_public/test_model.gltf`。

## 最终光照基线

角色使用 Evening Sky 环境与固定世界空间侧前方主光。当前 Endfield Look 将环境漫反射缩放为原基线的 58%，Key/Fill/Rim 分别为 1.38/0.10/0.24，shadow-map 可见度权重为 0.88，Lam 阴影色为 0.76。该组合保留环境可读性，同时避免填充光抹平角色与地台的实时阴影。
