# AzureRender 资产与视觉 QA

## 1. 资产分级

| 目录 | 用途 | 发布 |
|---|---|---|
| `assets_public/` | 自有测试模型、HDR、Ramp、Face SDF | 允许进入发布包 |
| `assets_private/` | 第三方角色和派生纹理 | 禁止公开分发 |
| `assets_placeholder/` | 不可分发资产的说明或占位 | 允许 |
| `captures/` | 本地生成截图、manifest、timing | 默认不提交 |
| `portfolio/` | 筛选后的公开作品集交付 | 按 manifest 审核 |

公共 smoke、CI 和发布验收不得依赖 `assets_private/`。

## 2. glTF 支持范围

- scene/node hierarchy、TRS/matrix
- triangle-list primitives
- POSITION/NORMAL/TANGENT/UV
- JOINTS_0、WEIGHTS_0、单 skin 和动画
- POSITION morph target
- Base Color、Normal、Metallic-Roughness、Emissive
- OPAQUE、MASK、BLEND、double-sided
- AzureRender 自定义材质 extras 和兼容 fallback

遇到不支持的 accessor、primitive mode 或 skin 数据时应明确报错，不能静默生成错误画面。

## 3. 私有角色导入原则

历史 Unreal 导出脚本位于 `tools/unreal_*.py`，纹理转换和 glTF 注入工具位于 `tools/`。脚本必须从自身路径或 `AZURERENDER_PROJECT_ROOT` 定位工程，不得写死本机目录。

导入检查：

1. 核对原资产许可和用途。
2. 导出 mesh、skin、animation 和材质纹理。
3. 转换 BC5 normal 和 packed metallic/roughness 数据。
4. 注入材质 extras，保留明确 fallback。
5. 用公共/私有资产分别跑 loader 和渲染 smoke。
6. 记录 primitive/material/vertex/index 数量。

## 4. 固定视觉 QA

角色视觉检查至少包含：

- 全身正面
- 左右/背面结构
- 脸部近景
- conventional/stylized A/B
- outline、shadow、normal 和 material isolation
- 动画与静态 pose

黑洞视觉检查至少包含：

- 1280x720 Beauty
- Photon Ring 与 Gravitational Lens diagnostic
- 静态 capture 重复性
- 时间序列中的噪声、拖影和 history reset
- HDR 高亮经 tone mapping 后不过曝成无细节色块

## 5. 自动化与人工验收边界

自动化负责：CLI、schema、数学、序列化、资源定位和确定性数据。人工截图检查负责：材质层次、轮廓稳定、噪声、残影、色带、构图和视觉回归。

视觉变化不能只以“无 Validation 错误”作为完成依据；同样，截图看起来正确也不能替代 Debug Validation。

## 6. 基准管理

- 基准必须记录分辨率、场景、资产、RenderSettings、capture fps/frame 和 SHA-256。
- 只有有意视觉变更才能更新基准。
- 更新前后都保留对比图或 contact sheet。
- 私有资产基准可以本地保存，但公共合并门禁必须有可重复的公共证据。

当前角色 S36 HDR Beauty SHA-256：

```text
5E8BF8B507FE07F385EAADF563DF40CD3C23FA6A2433156DEFD1BFD6AB829357
```

当前 P2 `Endfield Industrial` 公共资产 1280x720 基准：

```text
full-body: 71A76FA98B9E291BBF119700E4DCE58C9E643D02E3A27F2BC75FED004BC6FAB5
close-up:  7FB1D2C98BF87062E78CC7F92601B8616191CEC16A4AFAB13721DFB9CAC896AF
```

对应 manifest 必须包含 `qaShowcasePreset: 1`、`showcasePresetVersion: 1`、`showcasePresetName: "Endfield Industrial"` 及 grade/bloom/outline 完整参数。S36 仅保留为 P2 前的历史角色基准，不作为 P2 预设的期望哈希。

当前黑洞 P1 1280x720 首帧 SHA-256（36 帧、60 fps 固定捕获）：

```text
E750F12A7D585CCBD0613ECC2D6A50BC95103E8F8F41B532FC444448F910DC0C
```

两次完整运行必须逐帧一致；单次中断或未生成 manifest 的目录不作为基准。

## 7. 历史资料

详细的 Laevat 导入流水、CQ 阶段记录和 tone-mapping 设计快照已移入 `archive/`。它们可用于理解历史决策，但其中的路径、状态和下一任务不再有效。
