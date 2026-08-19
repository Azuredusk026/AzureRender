# 角色场景展示

## 当前交付

技术展示先播放 8 秒 Beauty 正常渲染，随后依次播放描边、世界法线、深度、底色、材质 ID、Hair KK 和阴影可见度，每个诊断视图 4 秒。Beauty 使用固定正前方相机和脚部中心转台；诊断段统一固定全身正面机位，便于逐项比较。画面原生输出 1600×900，编码强制 SAR 1:1，不做非等比拉伸。

| 文件 | 内容 | SHA-256 |
| --- | --- | --- |
| `images/20260819-170018_角色正常渲染.png` | Evening Sky、红色眉毛、实时阴影 | `77BAD32787BB3D19799218F9537966033F084117A19D467BAE52A923595114E0` |
| `images/20260819-170018_角色描边诊断.png` | 内部与轮廓边缘响应 | `47854D07A35641B2731499430CA927F28464AAEC7F455F97FC8421CBFF8857A1` |
| `images/20260819-170018_角色法线诊断.png` | 世界空间法线 | `C9986D5028869574526EA0160D8CBA59B803253A936C1676ED12849D11F0F72A` |
| `images/20260819-170018_角色深度诊断.png` | 线性深度层次 | `1DBAC09803581226EE798E0CBA9021E863E8506FCFCF9DAF996C87551DB8020E` |
| `video/20260819-170018_角色技术展示.mp4` | 864 帧、36.00 秒、8 个有序段落 | `D8D03B39ABA9BAB4618F4408A9108DE00CF2B423B90E3B2A34E54D9E8F85254B` |

生成环境：Debug Validation、NVIDIA GeForce RTX 4060 Laptop GPU、Evening Sky 环境、Endfield Industrial Look。视频已完整解码 864 帧，格式为 H.264 High、yuv420p/BT.709、1600×900、24 fps、SAR 1:1、DAR 16:9。

## 眉毛与头发数据

- `M_actor_laevat_brow_01` 使用 `brow-overlay` 专用 Unlit 通路：Face D RGB、0.95 不透明度，以及沿视线方向 `0.04679 m` 的顶点偏移；该数值对应原资产的 `4.679 cm`。
- `T_actor_laevat_hair_01_D`：头发 Base Color。
- `T_actor_laevat_hair_01_HN`：独立 Hair Data；RG 参与基础发束法线，BA 驱动双层 Kajiya-Kay 高光方向。
- `T_actor_laevat_hair_01_P`：Hair Master 的 `_P` packed 材质数据；注入器同时支持 `_P` 与布料使用的 `T_RGBA_P`。

## 复现

Beauty 使用 192 帧；其余段把 `--qa-isolation` 依次设为 `outline`、`world-normal`、`depth`、`albedo`、`material-id`、`hair-kk`、`shadow-visibility`，每段捕获 96 帧。

```powershell
.\build\ninja-debug\AzureRender.exe `
  --scene-type character `
  --asset .\assets_private\laevat_skinned\laevat_idle_material.glb `
  --portfolio --width 1600 --height 900 `
  --capture-dir .\build\character_beauty `
  --capture-frames 192 --capture-fps 24
```

私有角色模型、派生 GLB、纹理、截图和视频仅供本机视觉检查，不得提交 Git、进入 CI/安装包或公开 portfolio。公共自动化回归继续使用 `assets_public/test_model.gltf`。
