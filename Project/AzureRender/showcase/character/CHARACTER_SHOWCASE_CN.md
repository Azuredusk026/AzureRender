# 角色场景展示

## 当前交付

技术展示先播放 8 秒 Beauty 正常渲染，随后依次播放描边、世界法线、深度、底色、材质 ID、Hair KK 和阴影可见度，每个诊断视图 4 秒。Beauty 使用固定正前方相机和脚部中心转台，以 `0.40 rad/s` 覆盖正面、侧面和背面，使世界空间主光与实时阴影的变化可直接观察；诊断段统一固定全身正面机位，便于逐项比较。画面原生输出 1600×900，编码强制 SAR 1:1，不做非等比拉伸。

| 文件 | 内容 | SHA-256 |
| --- | --- | --- |
| `images/20260819-183550_角色红色眉毛近景.png` | Face D 驱动的独立深红眉毛近景证据 | `A9B28E5086E08A39DBEB6E173DF5DBE0437E33E06D7D11ABDE581DC09AFC2E92` |
| `images/20260819-183550_角色正面动态光照.png` | Beauty 正面受光与地台投影 | `A4C06EE42235E065AF31815812413980AFA052A0B03D32C37BA9431E5D0D6A37` |
| `images/20260819-183550_角色侧面明暗变化.png` | 转台侧面材质明暗变化 | `7051DE5BF85B8E8C21DFE58C4855CD4D8676FDCA0F6EF0208A2231E103E8A97D` |
| `images/20260819-183550_角色背面实时阴影.png` | 背面受光、衣料层次与实时阴影 | `CE60177CE426FEC198510D72BCB161755E85C53CA75E18AF2E68E778CECA2AF6` |
| `images/20260819-183550_角色描边诊断.png` | 内部与轮廓边缘响应 | `47854D07A35641B2731499430CA927F28464AAEC7F455F97FC8421CBFF8857A1` |
| `images/20260819-183550_角色法线诊断.png` | 世界空间法线 | `C4D2345544CA57D48A7FA1A35C2667820CED922D7E2D232107CDED0519C8EEF3` |
| `images/20260819-183550_角色深度诊断.png` | 线性深度层次 | `1DBAC09803581226EE798E0CBA9021E863E8506FCFCF9DAF996C87551DB8020E` |
| `video/20260819-183550_角色最终技术展示.mp4` | 864 帧、36.00 秒、8 个有序段落 | `A6C8FDE7D48E7FB3715390C9932622CAC7DA96F91125351C3901F2C8FDC77BAE` |

生成环境：Debug Validation、NVIDIA GeForce RTX 4060 Laptop GPU、Evening Sky 环境、Endfield Industrial Look。视频已完整解码 864 帧，格式为 H.264 High、yuv420p/BT.709、1600×900、24 fps、SAR 1:1、DAR 16:9。

## 眉毛与头发数据

- `M_actor_laevat_brow_01` 使用 `brow-overlay` 专用 Unlit 通路：Face D RGB、0.95 不透明度，以及沿视线方向 `0.04679 m` 的顶点偏移；该数值对应原资产的 `4.679 cm`。
- 针对导出后贴近上眼睑的眉毛卡片，材质数据额外提供 `0.01 m` 局部上移；Face D 结果乘深红显示补偿，保证眉毛在 HDR 合成后仍与眼线分离且可见。
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

## 最终光照基线

角色使用 Evening Sky 环境与固定世界空间侧前方主光。当前 Endfield Look 将环境漫反射缩放为原基线的 58%，Key/Fill/Rim 分别为 1.38/0.10/0.24，shadow-map 可见度权重为 0.88，Lam 阴影色为 0.76。该组合保留环境可读性，同时避免填充光抹平角色与地台的实时阴影。
