# Character 场景本机展示

## v2 视觉修正版

本版修复背景双网格、右侧白色灯柱、整体曝光不足、皮肤错误金属反光、Face SDF 灰色面罩感，并保持 Toon Ramp、AO 与双带 Hair KK 高光稳定。截图来自 Debug Validation 捕获，用于本机私有资产视觉验收；发布构建使用相同 SPIR-V 与 RenderSettings。

| 文件 | 机位 / 灯光 | SHA-256 |
| --- | --- | --- |
| `images/character_fullbody_endfield_v2_1920x1080.png` | full-body-front / stylized-key | `9EBFFDD5C8E6199D516E82D9DD95907E746D0D8756DE3390BF6479B948448DB7` |
| `images/character_threequarter_endfield_v2_1920x1080.png` | face-three-quarter / stylized-key | `99E21F9C62A45628969C8177FE3B7611CD1EAF5CBA9B3001245EA7305334A8EF` |
| `images/character_closeup_endfield_v2_1920x1080.png` | face-front / stylized-key | `EE2AD9F5D7D3B8F744D9DFBE4E96E1BE10476668CDE9B102D4D42C4A72F7D08D` |
| `images/character_back_endfield_v2_1920x1080.png` | back-detail / rear-emissive | `B08267EFB5955B9BE64A62F934CDEB401BDCA9EFB7935544F439431E49462345` |
| `images/character_material_neutral_v2_1920x1080.png` | face-three-quarter / neutral-material | `8F449CB582A52A8619F072FE52A4096A42B2C13BD97D1522C1FDB20534976026` |

视频 `video/character_portfolio_orbit_endfield_v2_1280x720_60fps.mp4` 为 300 帧、60 fps、5.0 秒、H.264/yuv420p/BT.709，SHA-256 为 `422740FCFE9DE9BF3DDDA08F195B066FB789F938B4F5F637529385943F310886`。

> 生成日期：2026-08-18
> GPU：NVIDIA GeForce RTX 4060 Laptop GPU

## 范围与授权

本组媒体使用本机 `assets_private/laevat_skinned/laevat_skinned_material_cq1_v2.glb`，只用于用户本机检查终末地式角色渲染。模型、纹理、截图和视频不得提交 Git、进入 CI、安装包或公开 portfolio。公共回归仍使用 `assets_public/test_model.gltf`。

## 截图

所有截图为 Release、1920x1080、Beauty 输出。

| 文件 | 机位 / 灯光 | SHA-256 |
| --- | --- | --- |
| `images/character_fullbody_endfield_v1_1920x1080.png` | full-body-front / stylized-key | `DED6A49F59B50F98F6A278235555036F175E106CB203CDA37CD02565EF85D17B` |
| `images/character_threequarter_endfield_v1_1920x1080.png` | face-three-quarter / stylized-key | `93DC1E81C1618B446373B834080A78F99FE503A26D589933CDF61CE8575AFCC7` |
| `images/character_closeup_endfield_v1_1920x1080.png` | face-front / stylized-key | `DD680F2127FA9AB91EF5B92C6C4F4CB098008BC451658AD52A1EB7F91ED1CBAD` |
| `images/character_back_endfield_v1_1920x1080.png` | back-detail / rear-emissive | `36910D4DAABB897CFDC1E130619249E9A75019EA534C96F1516A5A1514E6F940` |
| `images/character_material_endfield_v1_1920x1080.png` | full-body-front / neutral-material | `4ED1168F2D57FEB913B0562530D9C9C7F590EDD5243E811CB1325ADCE552C015` |

## 视频

`video/character_portfolio_orbit_endfield_v1_1280x720_60fps.mp4`：Release 固定 60 fps、180 帧、3.0 秒、H.264/yuv420p/BT.709，使用 portfolio orbit 连续展示多个方位。SHA-256：`6995A29E10EFEA872602CD18196D42A794F56B705949CC72A057B3A3F55AFA05`。

## 复现

```powershell
.\build\ninja-release\AzureRender.exe `
  --asset .\assets_private\laevat_skinned\laevat_skinned_material_cq1_v2.glb `
  --portfolio --width 1280 --height 720 `
  --capture-dir .\build\showcase-captures\character-video-beauty `
  --capture-frames 180 --capture-fps 60
```
