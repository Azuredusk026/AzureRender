# 角色场景展示

## 当前交付

本次使用固定正前方相机，角色围绕 Bind Pose 双脚中心旋转，展示底盘使用同一中心。世界空间主光保持不动，因此转台过程中可以观察到头发、皮肤、布料和金属部件的明暗变化以及底盘上的实时阴影。画面原生输出 1600×900，编码强制 SAR 1:1，不做非等比拉伸。

| 文件 | 内容 | SHA-256 |
| --- | --- | --- |
| `images/20260819-151945_角色正面全身.png` | 正面、双脚居中、红色眉毛 | `F08D1CC9553203A28667298C06190BB8CFE99C9041EF68C89CDA1EB73B193EDE` |
| `images/20260819-151946_角色四分之三光照.png` | 转台四分之三、材质明暗变化 | `40325B106D28929596273461322D6FDD79463E21F5C77029101EC78B20502D36` |
| `images/20260819-151947_角色背面阴影.png` | 背面材质与平台投影 | `C9E2B9A866A4915F07261B72B0866F841377D47F019BEF527B47C29D47512FEF` |
| `video/20260819-151950_角色正面转台展示.mp4` | 384 帧、16.00 秒、正面固定相机转台 | `5BF63F19B1FA45117FED36B529DB91127CEF560928B4BB587469533FF818D1A4` |

生成环境：Debug Validation、NVIDIA GeForce RTX 4060 Laptop GPU、Evening Sky 环境、Endfield Industrial Look。

## 眉毛与头发数据

- `M_actor_laevat_brow_01` 是透明 Overlay，使用 `T_actor_laevat_face_01_D` 的红色眉毛区域。透明三角索引修复后已在正面近景和正式视频中确认可见。
- `T_actor_laevat_hair_01_D`：头发 Base Color。
- `T_actor_laevat_hair_01_HN`：独立 Hair Data；RG 参与基础发束法线，BA 驱动双层 Kajiya-Kay 高光方向。
- `T_actor_laevat_hair_01_P`：Hair Master 的 `_P` packed 材质数据；注入器现同时支持 `_P` 与布料使用的 `T_RGBA_P`。

## 复现

```powershell
.\build\ninja-debug\AzureRender.exe `
  --scene-type character `
  --asset .\assets_private\laevat_skinned\laevat_idle_material.glb `
  --portfolio --width 1600 --height 900 `
  --capture-dir .\build\character_capture `
  --capture-frames 384 --capture-fps 24
```

私有角色模型、派生 GLB、纹理、截图和视频仅供本机视觉检查，不得提交 Git、进入 CI/安装包或公开 portfolio。公共自动化回归继续使用 `assets_public/test_model.gltf`。
