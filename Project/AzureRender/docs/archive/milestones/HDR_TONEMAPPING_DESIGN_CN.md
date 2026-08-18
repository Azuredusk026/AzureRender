# AzureRender HDR Scene Color 与 Tone Mapping 设计

> 设计节点：S36.1
> 日期：2026-08-02
> 状态：实施方案已冻结，尚未改变当前 Beauty 输出

## 1. 目的

S36 将当前直接写入 SRGB Swapchain 的 LDR 渲染路径升级为：

```text
Shadow Map
    ↓
Main Scene → RGBA16F Linear HDR Scene Color + Depth + World Normal
    ↓
Final Composite → Internal Outline → Exposure → ACES fitted
    ↓
SRGB Swapchain → HUD/Title/Fade → Present/Capture
```

目标不是接入 HDR 显示器或 HDR10 输出，而是在普通 SDR Swapchain 前保留线性高动态
范围，使 Emissive、Specular、Rim 和未来 Bloom 不会在 Main Pass 中提前截断。

## 2. 当前 S35 LDR 基线

当前 Surface Format 优先选择：

- `VK_FORMAT_B8G8R8A8_SRGB`；
- `VK_COLOR_SPACE_SRGB_NONLINEAR_KHR`。

Main Render Pass 的 Color Attachment 直接是 Swapchain Image。它同时写入 Depth 与
`VK_FORMAT_R8G8B8A8_UNORM` World Normal。Main Pass 结束后，Post-process Render
Pass 对同一个 Swapchain Image 使用 `LOAD`，以 Alpha Blend 叠加内部描边、诊断
画面和 HUD。因此当前没有可以被 Final Composite 采样的 Scene Color。

S36.1 冻结帧：

- `captures/s36_ldr_color_baseline/frame_000000.png`；
- 1920×1080 RGBA；
- SHA-256：
  `8FBCA478C47B094B25E6FEC3274FF6DACB6C18DF2817A77AEDCB092E9B5BA211`；
- 与 S30/S35 最终结构重构基准完全一致。

基线图像统计：

- RGB 最小值：16 / 24 / 26；
- RGB 最大值：255 / 210 / 221；
- RGB 平均值：39.073 / 63.402 / 69.578；
- 线性亮度 P50 / P90 / P95：0.043297 / 0.060778 / 0.096704；
- 线性亮度 P99 / P99.9：0.264917 / 0.316922；
- 任一通道 ≥250：0.000048%；
- 任一通道等于 255：0.000048%，即约一个像素；
- Alpha 非 255：0%。

这些数值描述最终 PNG，不代表 Main Shader 内部峰值；SRGB Swapchain 已经把高于 1.0
的线性输出截断。新 HDR 路径的价值是保留 Final Composite 之前的峰值，而不是单纯
把现有 PNG 调亮。

## 3. 已选择的格式与能力门槛

HDR Scene Color 固定使用：

```text
VK_FORMAT_R16G16B16A16_SFLOAT
```

理由：

- 每通道 16-bit Float，支持高于 1.0 和负值；
- 保留 Alpha，兼容现有 BLEND primitive；
- 相比 RGBA32F 显著降低带宽和显存；
- 比 R11G11B10_UFLOAT 更适合现有 Alpha Blend 路径；
- 是桌面实时渲染常用的 HDR 中间格式。

设备必须在 optimal tiling 下同时支持：

- `VK_FORMAT_FEATURE_SAMPLED_IMAGE_BIT`；
- `VK_FORMAT_FEATURE_COLOR_ATTACHMENT_BIT`；
- `VK_FORMAT_FEATURE_COLOR_ATTACHMENT_BLEND_BIT`。

S36.1 已加入启动能力探针。当前 RTX 4060 Laptop GPU 报告 `supported`，
`vulkaninfoSDK --show-formats` 也确认上述三项以及 Linear Filtering/Transfer 支持。
S36.2 启用 HDR 路径时，如果三项能力不完整，应在创建资源前明确失败，不进行静默
格式降级。Android/移动端 fallback 留到论文多设备能力层统一设计。

1080p 显存成本：

- 单张 RGBA16F：1920×1080×8 bytes，约 15.82 MiB；
- 三张 Swapchain 对应 Scene Color：约 47.46 MiB；
- Resize 时随 Swapchain Image Count 重新创建。

## 4. Render Pass 与资源变化

### 4.1 新增资源

`AzureRenderApp` 增加：

- `sceneColorFormat_`，默认 `VK_FORMAT_R16G16B16A16_SFLOAT`；
- `sceneColorImages_`；
- `sceneColorImageMemories_`；
- `sceneColorImageViews_`。

新增 `createSceneColorResources()`，数量与 Swapchain Image 一致，Usage 为：

```text
VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT
```

### 4.2 Main Render Pass

- Color Attachment 从 `swapchainFormat_` 改为 `sceneColorFormat_`；
- `storeOp = STORE`；
- final layout 改为 `VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL`；
- Main Framebuffer attachment 0 改为对应 `sceneColorImageView`；
- Depth 与 World Normal attachment 顺序保持不变；
- Background、Outline、Opaque、MASK、BLEND 仍在同一 Main Pass 中执行。

### 4.3 Final Composite Render Pass

- 仍写入 Swapchain Image；
- 不再 `LOAD` Main Scene，初始布局使用 `UNDEFINED`；
- 全屏 Final Composite Shader 保证覆盖整个 Render Area；
- 最终布局仍为 `PRESENT_SRC_KHR`；
- HUD/Title/Fade 继续在同一 Pass 中位于 Tone Mapping 之后。

### 4.4 Swapchain 生命周期

初始化与 Recreate 顺序：

```text
createSwapchain
createImageViews
createSceneColorResources
createDepthResources
createNormalResources
createRenderPass / createPostProcessRenderPass
createGraphicsPipeline
createFramebuffers / createPostProcessFramebuffers
createPostProcessDescriptorSets
```

`cleanupSwapchain()` 必须在销毁 Swapchain 前销毁 Scene Color View、Image 与 Memory，
并清空三个 vector。现有截图继续读取最终 Swapchain，不读取 HDR Scene Color。

## 5. Descriptor 方案

为减少 ABI 改动，保留现有 Post-process Binding：

- Binding 0：World Normal；
- Binding 1：Depth；
- Binding 2：Shadow Map。

新增：

- Binding 3：HDR Scene Color。

Post-process Descriptor Pool 的 Combined Image Sampler 数量由每图 3 个变为 4 个。
Scene Color 使用现有 `screenAttachmentSampler_` 的 nearest/clamp 配置，避免描边和
像素诊断引入额外过滤差异。后续 Bloom 会使用独立 Linear Sampler。

## 6. Final Composite Shader

`inner_outline.frag` 在 S36.2 升级为完整 Final Composite：

1. 采样 HDR Scene Color；
2. 计算现有 Depth/Normal Internal Outline；
3. Beauty 模式在线性 HDR 中把描边颜色与 Scene Color 合成；
4. 应用固定曝光；
5. 应用 ACES fitted；
6. 输出线性 0–1 到 SRGB Swapchain，由 Vulkan Attachment 自动执行 SRGB 编码。

严禁在 Shader 内再次执行 `pow(color, 1.0 / 2.2)`，否则会发生双重 Gamma。

Tone Mapping 选择 Narkowicz ACES fitted：

```glsl
vec3 acesFitted(vec3 color) {
    const float a = 2.51;
    const float b = 0.03;
    const float c = 2.43;
    const float d = 0.59;
    const float e = 0.14;
    return clamp(
        (color * (a * color + b))
            / (color * (c * color + d) + e),
        0.0,
        1.0);
}
```

初始曝光固定为 `0.0 EV`：

```glsl
color *= exp2(exposureEv);
```

选择原因：实现小、确定性强、可解释，能压缩 Emissive/Specular 峰值，并适合作为后续
AgX/Neutral 对比的基线。它不是完整 ACES 色彩管理系统，文档和作品集不得把它描述
成 ACES Reference Rendering Transform。

## 7. 描边、诊断视图与 HUD 规则

- Beauty：Internal Outline 在线性 HDR 中合成，然后 Tone Map；
- World Normal：绕过 Exposure/ACES，保持当前可读颜色语义；
- Internal Outline Diagnostic：绕过 Exposure/ACES；
- Shadow Map Diagnostic：绕过 Exposure/ACES；
- HUD、章节标题和 Fade：在 Final Composite 之后，以现有 LDR/SRGB 语义绘制；
- Final Composite Pipeline 不再启用 Alpha Blend，输出 Alpha 固定为 1；
- HUD Pipeline 保持 Alpha Blend；
- BLEND 材质仍在 HDR Main Pass 中按 primitive back-to-front 排序和混合。

## 8. Push Constant 与交互预留

`PostProcessPushConstants` 从 16 bytes 扩展到 32 bytes：

- `strength`；
- `depthThreshold`；
- `normalThreshold`；
- `diagnosticView`；
- `exposureEv`；
- `toneMappingEnabled`；
- 两个显式 padding float。

S36.2 默认 `exposureEv = 0.0`、`toneMappingEnabled = 1.0`。先保留 Tone Mapping
开关用于 A/B 验证；曝光热键与 HUD 显示可在 S36.3 加入，避免第一步同时扩大交互面。

## 9. 验收标准

### 9.1 功能门槛

- Debug/Release 构建；
- 启动明确打印 HDR Scene Color Format 支持；
- 公共资产 120 帧 Debug Validation；
- Resize/minimize/restore 无资源泄漏或 Validation Error；
- 私有角色 25 帧五章节技术序列；
- Screenshot/PNG Capture 仍为 8-bit RGBA、Alpha 全 255；
- World Normal、Internal Outline、Shadow Map 不受 Tone Mapping 污染。

### 9.2 视觉门槛

S36.2 会有意改变 Beauty 像素，因此不再要求匹配 S35 SHA-256。必须：

- 保存 `S36 HDR Beauty v1` 新基准与 SHA-256；
- 对比全身、脸部近景、金属/武器、头发、Emissive 和地台阴影；
- 不出现整体灰雾、肤色明显偏色、背景黑位抬升或高光大面积纯白；
- 隐藏口腔/手臂仍不得穿透衣物；
- 输出任一通道等于 255 的像素比例目标低于 0.01%；
- Alpha 非 255 必须为 0%；
- 记录 RGB 平均值与线性亮度 P50/P90/P99/P99.9，不以单一平均亮度判断优劣。

### 9.3 性能门槛

- 25 帧 Debug 只验证 Timestamp Query 正常，不作性能结论；
- 正式比较使用 1920×1080、Release、固定设备、固定驱动、预热后 600 样本；
- Post-process Timestamp 会同时包含 Scene Color Sample、Outline 与 Tone Mapping；
- 记录新增 Scene Color 显存估算和 Post-process GPU 增量。

## 10. 分步实施

### S36.2：HDR Scene Color + Fixed Tone Mapping

- Scene Color Resource/Lifecycle；
- Render Pass/Framebuffer 改线；
- Binding 3 与 Descriptor 更新；
- Final Composite Shader；
- 0 EV ACES fitted；
- 完整功能/视觉回归并建立新 Beauty 基准。

### S36.3：曝光交互与正式性能回归

- Exposure 热键和 HUD 显示；
- Tone Mapping A/B；
- 600 Sample Release GPU Timing；
- 代表帧 Contact Sheet 与作品集说明更新。

### 后续

- Bloom 使用 HDR Scene Color 构建独立下采样链；
- HDR Environment/Prefiltered IBL；
- 移动端格式能力与带宽 fallback；
- AgX/Khronos Neutral 作为可选 Tone Mapper，而不是在 S36.2 同时引入。

## 11. S36.2 实施结果

S36.2 已按本文冻结方案完成：RGBA16F Scene Color 生命周期、Main/Final Attachment
改线、Post-process Binding 3、32-byte Push Constant、HDR 描边合成、0 EV ACES fitted、
诊断视图绕过和 Tone Mapping 后 HUD 均已启用。

新基准：

- `captures/s36_hdr_beauty_v1/frame_000000.png`；
- SHA-256：
  `5E8BF8B507FE07F385EAADF563DF40CD3C23FA6A2433156DEFD1BFD6AB829357`；
- RGB min 5/11/13，max 235/217/222，mean 29.000/60.084/69.169；
- Linear Luminance P50/P90/P95/P99/P99.9：
  0.037679/0.062127/0.120969/0.394080/0.457723；
- RGB 255 像素比例 0%，Alpha 非 255 比例 0%。

Debug/Release、公共 120 帧 Validation、实际窗口最大化/最小化/恢复、私有 Beauty 和
25 帧五章节技术序列均通过。
短 Debug 技术探针的 Final Composite 平均 0.142 ms；该数字只证明 Timestamp 路径
工作，正式性能结论留给 S36.3 的 Release 预热长样本。
