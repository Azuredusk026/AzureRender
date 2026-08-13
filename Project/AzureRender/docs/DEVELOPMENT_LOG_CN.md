# 开发日志

## 2026-07-24

### S7：莱万汀静态角色首次接入

- 从 Unreal 5.7 导出参考姿势静态 GLB。
- glTF 场景遍历结果：81,198 顶点、283,809 索引、13 primitives、14 materials。
- 按材质实例名称恢复 11 个 Base Color 绑定。
- 增加 alpha mode、double-sided、自动包围盒居中与取景。
- Debug/Release 构建和 120 帧 smoke test 通过。

### S8-A：BC5 Normal 恢复

- 确认 Unreal `_N` PNG 的 B 通道恒为 0，数据实质为 BC5。
- 新增无第三方运行依赖的 PNG 解码/编码与 Z 分量重建。
- 翻转 Unreal/DirectX 绿色通道后嵌入 glTF。
- Vulkan 使用线性 UNORM 纹理执行切线空间法线映射。
- 视觉检查未发现明显法线翻转或 UV 接缝爆裂。

### S8-B：服装/武器物理材质通道

目标：先完成一条可解释、可验证的常规渲染路径，再继续风格化功能。

完成：

- 扩展 Unreal 提取脚本，记录父材质、标量与向量参数。
- 新增母材输出端追踪脚本 `tools/unreal_trace_cloth_material.py`。
- 确认 `_P` 为 MSRE 打包图：R Metallic、G Specular、B Roughness、A Emissive mask。
- 确认 `_M` 为带颜色和 offset 参数的风格遮罩，不作为 Metallic。
- 将 `_P.R/_P.B` 转换成标准 glTF Metallic-Roughness B/G 通道。
- 5 个服装/武器材质成功绑定 Metallic-Roughness。
- Vulkan descriptor set 从 2 张扩展到 3 张材质纹理。
- UBO 增加相机世界坐标，顶点着色器输出 world position。
- 片元着色器增加 roughness-driven specular 与 metallic F0 tint。

验证：

- Debug 与 Release 均重新编译成功。
- Debug Validation Layer 连续渲染 120 帧，无错误。
- Release 连续渲染 120 帧，无错误。
- 视觉 QA：角色、武器和服装正常旋转；未发现新法线翻转、UV 爆裂或贴图错绑。

已知限制：

- 当前没有 IBL，深色金属缺少环境反射，因此部分黑色服装偏暗。
- `_P.G` Specular、`_P.A` Emissive mask、独立 `_E` 和 `_M` 尚未接入。
- 透明材质仍未按相机深度排序。

下一节点：

- 优先加入基础 IBL/环境反射，解决深色金属在截图中不可读的问题。
- 随后完成 Emissive 和透明排序，形成作品集截图可用的常规渲染节点。

### S9：基础环境光与反射

目标：补齐作品集常规渲染节点所需的环境照明，让深色服装和金属在没有复杂场景的情况下仍有可读层次。

完成：

- 在渲染器启动时生成 512×256 线性经纬环境纹理。
- 环境包含冷色天空、暗色地面、地平线补光和与主方向光一致的亮区。
- 新增全局 Environment GPU Texture，并通过 binding 4 共享给全部材质描述符。
- 片元着色器根据世界空间法线采样环境漫反射。
- 根据视线和法线计算 Reflection Vector，并加入 Schlick Fresnel。
- Roughness 控制反射方向展宽、平均环境混合和反射能量。
- 保留直接漫反射与 Blinn-Phong 高光，形成直接光加环境光的常规路径。
- 自动取景比例由 `2.0 / extent` 调整为 `2.5 / extent`，提高角色在作品集截图中的占屏比例。

调试记录：

- 首次视觉检查过暗。原因是环境辐射纹理错误使用 sRGB 格式，低亮度值在线性化后被显著压低。
- 将环境资源改为 `VK_FORMAT_R8G8B8A8_UNORM` 后恢复线性辐射强度。
- 第二轮加入非金属环境照明下限和金属色补偿，避免黑色材质只剩轮廓。

最终验证：

- Debug 与 Release 构建成功。
- 莱万汀 Debug Validation Layer 连续渲染 120 帧，无错误。
- 莱万汀 Release 连续渲染 120 帧，无错误。
- 公共测试模型 Debug 连续渲染 120 帧，无错误。
- 正面及旋转角度视觉 QA 通过；服装褶皱、武器和金属边缘出现稳定环境响应。
- 未发现过曝闪烁、法线翻转、UV 接缝或描述符错误。

当前限制：

- 环境纹理为 LDR 程序生成纹理，不是外部 HDRI。
- Roughness 模糊目前是方向展宽和平均环境混合近似，尚未生成 prefiltered specular mip chain。
- 暂无 irradiance cubemap、BRDF LUT 和曝光控制。

下一节点：

- S10 接入 `_P.G` Specular 与 `_P.A × _E` Emissive。
- 随后处理透明排序，并增加固定相机和作品集截图控制。

### S10：Specular 与 Emissive

目标：恢复 `_P` 剩余的 G/A 通道，使服装材质的非金属高光强度和局部发光不再丢失。

资产分析：

- 5 个 cloth/weapon 材质使用 `T_RGBA_P`。
- cloth_01 使用独立 `_E`，Unreal `_E_Strengh = 50`。
- cloth_02 使用独立 `_E`，Unreal `_E_Strengh = 10`。
- cloth_03/04/05 只有 `_P`，没有 `_E`。
- 两张 `_E` 都是稀疏彩色纹理，有效区域约占 0.7%–3%。
- `_P` 分辨率为 2048²，`_E` 分辨率为 1024²。

完成：

- 新增 `_E` 按 UV 重采样到 `_P` 尺寸的离线转换。
- 生成 Specular-Emissive 复合纹理：RGB = `_E.rgb × _P.a`，A = `_P.g`。
- RGB 使用 sRGB 采样，Alpha 自动保持线性，兼顾颜色与数据通道。
- 5 个材质接入 Specular，其中 cloth_01/02 接入 Emissive。
- S10 GLB 为 58,460,840 bytes，包含 18 张去重嵌入纹理。
- `AssetMaterial` 增加 Specular-Emissive 像素与 Emissive Strength。
- Vulkan 每材质描述符新增 binding 5。
- 材质 push constant 增加 Emissive Strength。
- dielectric F0 从固定 0.04 改为 `0.04 × Specular`。
- Emissive 在完整直接光与环境光之后叠加，shader 显示倍率为 3。

验证：

- Debug 与 Release 重新编译成功。
- 莱万汀 Debug Validation Layer 120 帧通过。
- 莱万汀 Release 120 帧通过。
- 公共测试模型 Debug 120 帧通过，回退纹理的 Specular 为 1、Emissive 为 0。
- 放大窗口检查正面和背面：发光区域保持局部、无大面积过曝；金属与服装高光连续。
- 复合纹理统计：cloth_01 发光有效像素约 2.872%，cloth_02 约 0.617%。
- 未发现 UV 错位、描述符错误或 push constant 校验错误。

当前限制：

- 尚未实现 HDR Framebuffer、Tone Mapping 与 Bloom，因此 Emissive 只表现为自发光颜色，不产生光晕。
- Specular 暂未实现 `KHR_materials_specular`，而是项目内复用 glTF Emissive Texture 的 Alpha 通道。
- `_M` 风格遮罩仍未进入 shader。

下一节点：

- S11 修复透明材质排序，并检查头发、眉毛和眼影。
- 随后增加固定相机、暂停旋转和作品集截图控制。

### S11：透明 Primitive 排序

目标：修复角色旋转时多个 BLEND 材质之间可能出现的错误覆盖，同时保持不透明材质的 Early-Z 与稳定深度。

资产盘点：

- 当前 GLB 有 13 个有效材质、13 个 mesh primitives。
- BLEND 材质只有 3 个：`M_hairshadow_common_01_001`、`M_actor_laevat_brow_01`、`M_eyeshadow_common_01`。
- 头发主体 `M_actor_laevat_hair_01` 是 OPAQUE。
- 服装和裙摆当前均为 OPAQUE，没有将其误改为透明。

完成：

- `AssetPrimitive` 增加导入后空间包围盒中心。
- Loader 在节点变换应用后，按 primitive 顶点范围计算中心。
- 每帧保存与 UBO 完全一致的 Model Matrix。
- 所有 OPAQUE/MASK primitive 继续按资产顺序优先绘制。
- 收集 BLEND primitive，并根据当前 Model Matrix 转换中心。
- 使用相机前向向量计算严格视空间深度。
- 使用 `stable_sort` 从远到近提交透明 primitive。
- BLEND pipeline 保持 Depth Test 开启、Depth Write 关闭和标准 SrcAlpha 混合。

验证：

- Debug 与 Release 重新编译成功。
- 莱万汀 Debug Validation Layer 120 帧通过。
- 莱万汀 Release 120 帧通过。
- 公共测试模型 Debug 120 帧通过。
- 最大化窗口跨背面、侧面和旋转过渡角度检查，没有新增透明层跳变、穿透或整片消失。
- 透明排序不会改变 OPAQUE 材质或现有 descriptor 路径。

当前限制：

- 排序粒度为 primitive，不会在单个 primitive 内对三角形重新排序。
- 当前角色的三个 BLEND primitive 都是相对局部的面部/发影叠层，primitive 排序足够作为现阶段基线。
- 大面积交叉透明网格未来可能需要拆分 primitive、per-triangle sorting、weighted blended OIT 或 depth peeling。

下一节点：

- S12 增加暂停旋转、固定正面/侧面/背面角度和作品集截图控制。
- 控制功能完成后再进入屏幕空间描边。

### S12：作品集视角与原生截图

目标：让当前常规渲染节点可以稳定复现相同角色角度，并直接输出不带窗口边框的作品集画面。

交互控制：

- `Space` 切换自动旋转/暂停。
- `R` 恢复自动旋转。
- `1/2/3/4` 切换正面、右侧、背面、左侧并自动暂停。
- `Left/Right` 每次微调 5°并自动暂停。
- `F12` 请求下一帧截图。

角度校准：

- 初版采用 0°/90°/180°/270°，实际因为相机处于对角方向而得到四个三分之四视角。
- 最终给全部预设加入 45°基础偏移。
- 实际窗口确认：1 为正面、2 为右侧、3 为背面、4 为左侧。

Vulkan 截图路径：

- 创建 Swapchain 时增加 `VK_IMAGE_USAGE_TRANSFER_SRC_BIT`。
- F12 请求帧创建 Host Visible/Coherent Transfer Destination Buffer。
- Render Pass 结束后将 Swapchain Image 从 Present Layout 转到 Transfer Source。
- 使用 `vkCmdCopyImageToBuffer` 读回当前帧。
- 等待当前 frame fence 后映射内存。
- 根据 Swapchain Format 执行 BGRA→RGBA 转换。
- 使用 stb_image_write 保存为 `captures/capture_<timestamp>.png`。
- 将 Swapchain Image 转回 Present Layout 后正常显示。

验证：

- Debug 与 Release 构建成功。
- 角色 Debug、角色 Release、公共测试模型 Debug 各 120 帧通过。
- Release 下测试了四个预设、左右微调和恢复旋转。
- Debug Validation Layer 下触发 F12 并继续渲染，stderr 无 validation 消息。
- 成功生成 1280×720 RGBA PNG，文件约 240 KB。
- 直接读取 PNG 确认无上下翻转，红蓝通道正确，且不包含窗口标题栏。
- `captures/` 已加入 `.gitignore`，避免私有角色截图进入公开仓库。

下一节点：

- S13 实现作品集所需的角色描边。
- 优先选择几何外扩描边作为当前 Forward 路径的稳定基线，再评估屏幕空间描边。

### S13：Inverted-Hull 几何描边

目标：在不引入额外 G-Buffer/Post Process 的情况下，为当前 Forward Renderer 增加稳定可展示的角色轮廓。

完成：

- 新增独立 `outline.vert` / `outline.frag`。
- Camera UBO 增加 Rendering Parameters。
- 描边宽度使用 `largestExtent × 0.004`，适配不同模型尺寸。
- 新增 Outline Graphics Pipeline。
- Outline Vertex Input 只声明 Position 和 Normal。
- Front Face Culling 保留外扩壳层背面。
- 深蓝黑纯色 fragment 输出。
- Outline 在主材质之前绘制。
- 仅处理 OPAQUE/MASK，跳过三个 BLEND 面部叠层。

调试记录：

- 初版 Outline 开启 Depth Write，外扩壳层先占据深度，导致主模型大面积无法通过 `LESS`，画面接近全黑。
- 修正为 Depth Test 开启、Depth Write 关闭后，主材质正常覆盖壳层内部，只保留外轮廓。
- 初版 Outline Pipeline 沿用四属性 Vertex Input，校验层提示 Tangent/UV 未被消费。
- 将 Outline Vertex Attribute Count 改为 2 后，Debug Validation 完全干净。

验证：

- Debug 与 Release 构建成功。
- 莱万汀 Debug Validation Layer 120 帧通过，无 warning/error。
- 莱万汀 Release 120 帧通过。
- 公共测试模型 Debug 120 帧通过。
- 正面、侧面、背面最大化窗口检查通过。
- 描边没有壳层覆盖、宽度跳变或顶点尖刺爆炸。
- 成功通过 F12 保存 2560×1334 高 DPI 描边截图。

关于“角色半透明”的诊断：

- 主体材质均为 OPAQUE，混合关闭且 shader Alpha 固定为 1。
- 只有发影、眉毛、眼影使用 BLEND。
- 视觉上的半透明感来自过强的冷灰环境反射、未应用 Unreal 材质实例强度、缺少 `_M`/Matcap/AO/风格化阴影，以及服装本身的大量几何间隙。
- 结论：这是材质对比和反射能量问题，不是角色主体 Alpha 问题。

下一节点：

- S14 应优先修正材质实例强度和环境反射能量，消除洗灰/玻璃感。
- 随后接入 `_M` 风格遮罩，再决定是否增加屏幕空间内轮廓。

### S14：材质实例强度与环境反射校正

目标：修正角色在 S13 画面中大面积洗灰、类似玻璃或半透明的观感，同时保持现有 OPAQUE/BLEND 分类不变。

参数核对：

- `cloth_01`：Metallic 0.5，Roughness Adjustment +0.5。
- `cloth_02`：Metallic 0.5，Roughness Adjustment +0.3。
- `cloth_03`：Metallic 使用父材质默认 0.5，Roughness Adjustment 0.0。
- `cloth_04 / weapon`：Metallic 0.5，Roughness Adjustment -0.7。
- `cloth_05`：Metallic 0.5，Roughness Adjustment +0.3。

完成：

- `convertUnrealMsreToGltfMrPng` 新增逐材质参数。
- Metallic 输出改为 `_P.R × GGX_Metallic_Strengh`。
- Roughness 输出使用 `clamp(_P.B + Roughnessmap_Strengh × 0.25, 0.08, 1.0)`。
- 四分之一尺度是缺少完整母材函数时的保守近似，避免武器的 -0.7 直接造成大面积零粗糙度。
- 转换缓存键加入输入贴图、Metallic Strength 和 Roughness Adjustment，修复共享 `_P` 的 cloth_02/03/05 被错误复用同一结果的问题。
- 环境镜面反射能量由 `mix(1.15, 0.35, roughness)` 下调到 `mix(0.70, 0.20, roughness)`。
- 金属 Base Color 能量补偿由 `mix(0.18, 0.05, roughness)` 下调到 `mix(0.06, 0.015, roughness)`。
- 重新生成 60,561,264 bytes 的材质 GLB，包含 19 张去重内嵌纹理。

验证：

- Debug 与 Release 着色器构建成功。
- 莱万汀 Debug Validation Layer 120 帧通过，无 warning/error。
- 莱万汀 Release 120 帧通过。
- 公共测试模型 Debug 120 帧通过。
- 最大化窗口并使用数字键 1 固定正面机位，F12 成功保存 2560×1334 原生交换链截图。
- S13 对照图：`captures/capture_1784903926858.png`。
- S14 新图：`captures/capture_1784904559877.png`。
- 对比结果：肩甲、裙装和武器的冷灰高光明显收敛，暗部更稳定；肤色与基础色亮度基本保留，不是简单整体压暗。

“半透明感”结论更新：

- 主体仍然是 OPAQUE，Blend 关闭且输出 Alpha 为 1。
- S14 证明主要问题确实来自材质实例强度缺失与环境反射能量偏高。
- 目前残留的脸部灰块、薄片感和明暗层不足，主要来自未接入的 `_M`、Matcap、AO Color、Lam Shadow Color，以及少量真实几何间隙和三个 BLEND 面部/发影叠层。

后续更正：以上判断遗漏了局部三角形被背面剔除造成的真实遮挡孔洞，详见 S14.1。

下一节点：

- S15 接入 `_M` 风格遮罩，先恢复可控的角色明暗分层和局部材质对比。
- 在 `_M` 路径稳定后，再评估 Matcap/AO Color、Bloom 与屏幕空间内轮廓的优先级。

### S14.1：双面遮挡修复

问题复核：

- 用户指出 S14 正面截图中可以从头部看见口腔内部，从衣服看见手臂。
- 该现象是实际的后方几何泄漏，不应只归因于环境反射或材质洗灰。
- OPAQUE 管线经检查保持 `blendEnable = false`、Depth Test 开启、Depth Write 开启、输出 Alpha 1。

根因：

- GLB 根节点变换行列式为正，不存在整体镜像。
- 三角形几何法线与导出顶点法线统计：
  - hair：5,782 / 14,340 个三角形方向不一致，约 40.3%；
  - face：3 个方向不一致；
  - cloth_03：27 个方向不一致；
  - cloth_01：14 个方向不一致。
- 单面 Back-Face Culling 会移除局部近表面，产生可以直接看见口腔和身体的孔洞。

修复：

- 在 Laevat GLB 材质注入阶段，将所有 OPAQUE/MASK 材质设为双面。
- BLEND 发影、眉毛和眼影仍保留现有透明排序策略。
- 双面材质仍写入深度；该修复补回遮挡表面，不是开启 Alpha 混合。
- 重新生成的 GLB 为 60,561,416 bytes，包含 19 张去重内嵌纹理。

验证：

- Debug Validation Layer 120 帧通过，无 warning/error。
- Release 120 帧通过。
- 最大化窗口检查数字键 1 正面与数字键 2 右侧机位。
- 正面原先可见的圆形口腔结构已消失。
- 脸部、肩甲、袖口和裙装的前后遮挡明显恢复。
- 修复后原生交换链截图：`captures/capture_1784905076440.png`。

结论：

- 用户对“角色确实存在半透明/穿透现象”的观察是正确的。
- 精确说法是：主体没有使用 Alpha Blend，但局部近表面被错误剔除，造成了与半透明相同的后方结构可见结果。
- S14.1 已修复这类绕序/剔除孔洞；剩余观感问题再交由 S15 `_M` 风格遮罩处理。

### S15：`_M` 风格遮罩与分段漫反射

目标：在现有 Forward Renderer 中恢复第一层角色专用风格控制，让服装结构边缘和直接光明暗分界不再完全依赖连续 PBR 响应。

资产分析：

- 实际 `_M` 只存在于 cloth_02 和 cloth_05。
- 两张纹理均为 1024×1024 单通道灰度 PNG。
- cloth_02 `_M`：96.30% 像素为 0，最大值 193。
- cloth_05 `_M`：97.69% 像素为 0，最大值 255。
- 亮区沿纹理岛和服装结构边缘分布，适合作为局部风格强调遮罩。
- 两个实例的 `_M_Color` 均为 `(7.0, 0.35, 0.0, 1.0)`；考虑到 R 大于 1，本阶段不把它直接当作普通颜色。

完成：

- PNG 解码工具支持 8-bit grayscale、Truecolor 与 RGBA。
- 注入器将两张 `_M` 嵌入 GLB，并通过内部 `occlusionTexture` 约定传递。
- `AssetMaterial` 新增 Style Mask 像素、宽度和高度。
- 无 Style Mask 材质自动使用 2×2 黑色回退。
- `GpuMaterial` 新增 Style Mask GPU Texture。
- Descriptor Set Layout 新增 binding 6。
- Descriptor Pool 每材质组合采样器从 5 增加到 6。
- Descriptor Write 从 6 项增加到 7 项。
- Shader 使用 `smoothstep(0.08, 0.62, mask)` 提取有效边缘。
- 遮罩亮区叠加低强度暖色强调。
- 直接漫反射改为 `smoothstep(0.28, 0.52, N·L)` 平滑分段。
- 新 GLB 为 60,674,036 bytes，包含 2 个 Style Mask 材质与 21 张去重纹理。

视觉 QA：

- 固定正面和右侧机位检查通过。
- cloth_02/05 的边缘出现可读但不过曝的暖色强调。
- 肩甲、腰部与裙装的明暗分区更集中。
- 没有把整件衣服染色，没有新增皮肤断层。
- S14.1 的口腔、手臂遮挡修复保持有效。
- 原生交换链截图：`captures/capture_1784905639082.png`。

回归：

- 莱万汀 Debug Validation Layer 120 帧通过，无 warning/error。
- 莱万汀 Release 120 帧通过。
- 公共测试模型 Debug 120 帧通过，Style Mask 黑色回退正常。
- Debug 与 Release 全量重建通过。

下一节点：

- S16 增加运行时风格参数，至少支持 `_M` 强度和分段光照阈值调节。
- 随后恢复 AO Color、Lam Shadow Color 或 Matcap 中的一项，优先选择能明显改善脸部和布料体积感的路径。

### S16：运行时风格参数与同机位对比

目标：把 S15 的 shader 常量变成可现场演示的运行时参数，并生成只改变风格状态、不改变相机和资产的作品集 before/after。

完成：

- `VulkanApp` 新增：
  - `stylizedLightingEnabled_ = true`；
  - `styleMaskStrength_ = 1.0`；
  - `diffuseBandThreshold_ = 0.40`。
- Camera UBO `renderingParameters` 现在携带：
  - X Outline Width；
  - Y Style Mask Strength；
  - Z Diffuse Band Threshold；
  - W Diffuse Band Softness 0.12。
- Shader 使用 Z 的负值作为常规光照哨兵。
- 风格关闭时 `_M` 强度归零，漫反射恢复连续 `N·L × 0.55`。
- 风格开启时使用平滑分段漫反射与 `_M` 边缘强调。

最终控制：

- `F9`：风格化开关。
- `F7/F8`：Style Mask Strength -/+ 0.1，限制在 0–2。
- `F5/F6`：Diffuse Band Threshold -/+ 0.05，限制在 0.05–0.95。

交互问题与修正：

- 初版使用 `T`、`[`/`]`、`,`/`.`。
- 中文输入法候选层截获字母和标点，第一次自动化对比只保存了一张截图。
- PageUp/PageDown 与 Up/Down 在当前输入自动化层也没有稳定送达 GLFW。
- 最终统一改用 F5–F9；日志确认 F9 开关、F8 强度和 F6 阈值均实际触发。

作品集证据：

- 开启截图：`captures/capture_1784906439658.png`。
- 关闭截图：`captures/capture_1784906454927.png`。
- 两张图均为 2560×1334、正面预设 1、同一窗口尺寸。
- 变化像素 19,076，占全图 0.559%。
- 强变化像素 3,867，占全图 0.113%。
- 最大单通道差值 61，平均每通道绝对差 0.0273。
- 差异集中在角色的结构边缘与光照分界，背景和构图没有变化。

验证：

- F8 日志：`Style mask strength: 1.1`。
- F6 日志：`Diffuse band threshold: 0.45`。
- 莱万汀 Debug Validation Layer 120 帧通过，无 warning/error。
- 莱万汀 Release 120 帧通过。
- 公共测试模型 Debug 120 帧通过。
- Debug/Release 最终重建通过。

下一节点：

- S17 优先恢复 Lam Shadow Color 与 AO Color，改善皮肤、头发和大面积布料的体积感。
- 保留 F9 作为后续所有风格层的总开关，让作品集对比持续可复现。

### S17：逐材质 Lam Shadow Color 与 AO Color

目标：把 Unreal 材质实例中已有的角色阴影色接入当前 Forward + Toon 路径，以低风险方式补充材质体积感，并保持 F9 的常规/风格化同机位对照。

完成：

- 审核角色材质参数，兼容 `Lam_Shadow_Color` 与 `Lam_ShadowColor` 两种命名；
- 注入器把有效的 `AO_Color` 和 Lam Shadow Color 写入 glTF material extras；
- 全零、缺失或非法颜色使用中性回退，避免脸部在缺少遮罩时被整体染黑；
- Loader 读取两个逐材质 RGBA 参数；
- Material Push Constants 扩展至 48 bytes，并增加编译期大小校验；
- Shader 仅在 toon 阴影带中混合 Lam 0.35 与 AO 0.20；
- F9 关闭时两层自动归零，连续光照、纹理、相机与构图保持不变；
- 重新生成 Laevat GLB：60,675,256 bytes，11 Base Color、7 Normal、5 MR、5 Specular、2 Emissive、2 Style Mask、7 AO Color、6 Lam Shadow Color、21 张去重纹理。

视觉 QA：

- 正面固定机位中，头发、深色衣裙和武器获得克制的材质阴影色分层；
- 身体暖色 AO 没有污染亮部肤色；
- F9 开关确认差异只来自风格阴影响应；
- 右侧机位未发现头部内部、口腔结构或手臂穿衣回归；
- 正面证据：`captures/capture_1784907473858.png`；
- 右侧证据：`captures/capture_1784907508007.png`。

回归：

- 莱万汀 Debug Validation Layer 120 帧通过；
- 莱万汀 Release 120 帧通过；
- 公共测试模型 Debug 120 帧通过；
- Debug/Release 全量构建通过。

结论：

- S17 不透明度和遮挡逻辑保持不变；新增效果是逐材质阴影带染色，不是 Alpha 半透明。
- 当前权重足以产生可辨识差异，但不会把头发或黑色布料压死，因此保留 Lam 0.35 / AO 0.20。

下一节点：

- S18 先审核脸部贴图与材质参数，确认是否存在可用的 Face SDF/Shadow Mask。
- 有可靠数据则实现视角/光照相关的脸部阴影；没有则记录资产限制并转向逐材质 Matcap。

### S18：脸部 SDF 审计与局部 Matcap

决策：

- 脸材质虽然存在 `SDF_Location` 与 `SDF_Color` 参数，但没有 SDF Texture；
- 莱万汀目录没有 Face SDF/Shadow Mask；
- Face Base Color Alpha 近乎全白，仅含边缘抗锯齿，不是打包距离场；
- 按计划放弃无数据依据的 Face SDF，转向材质真实绑定的 `Matcap01`。

完成：

- 新增 Unreal Python 导出工具 `unreal_export_laevat_matcap.py`；
- 从 `/Game/Matcap/Matcap01` 导出 256×256 PNG；
- 注入器识别外部 Matcap 路径并写入 glTF extras；
- 新 GLB 内嵌 1 个 Matcap，总计 22 张去重纹理；
- `AssetMaterial` 和 `GpuMaterial` 增加 Matcap 数据；
- Descriptor binding 增加至 7；
- 每套材质采样器增加至 7 个；
- 无 Matcap 材质使用黑色 2×2 回退；
- Push Constants 扩展至 64 bytes；
- Shader 使用视空间法线生成 Matcap UV；
- 以 0.10 的保守强度叠加脸部局部高光；
- F9 继续作为该效果的总开关。

视觉 QA：

- 正面与右侧固定机位通过；
- 没有白块、纹理重复、头发误采样或遮挡回归；
- 与 S17 同机位基线相比变化 269 像素；
- 变化范围 `(1288,326)–(1357,396)` 完整落在脸部；
- 最大单通道差 40；
- S18 截图：`captures/capture_1784908447255.png`；
- F9 关闭对照：`captures/capture_1784908458610.png`。

回归：

- 莱万汀 Debug Validation Layer 120 帧通过；
- 莱万汀 Release 120 帧通过；
- 公共测试模型 Debug 120 帧通过；
- Debug/Release 全量重建通过。

结论：

- S18 恢复的是原材质真实引用的脸部局部 Matcap，不是猜测的 Face SDF。
- 效果范围严格限制在脸部，适合作为当前静态角色资产的可靠基线。

下一节点：

- S19 增加近景/脸部作品集机位及对应截图控制。
- 近景完成后再审计 Hair Matcap、`_HN` 或展示背景。

### S19：脸部近景预设与 Matcap 近景修正

目标：增加可复现的头肩近景，让脸部、眼睛、发影和 Matcap 能以作品集分辨率直接检查，同时保持全身机位不变。

相机实现：

- 新增数字键 `5`；
- 进入正面 45° 模型角度并暂停旋转；
- Camera Position 使用 `(0.915, 1.507, 1.046)`；
- Camera Target 使用 `(0, 0.82, 0)`；
- 不修改模型归一化比例与描边尺度；
- 数字键 `1–4` 恢复全身 Camera Position、Target 和角度；
- Matcap 相机 Forward 改为逐 fragment 的真实观察方向。

近景修正：

- 首次近景检查发现 S18 黑白 Matcap 在左脸形成硬边白色胶囊；
- F9 对照确认该形状来自 Matcap；
- 使用 9-tap 加权采样软化边缘；
- 使用 Base Color 对 Matcap 提亮进行肤色调制；
- 最终强度从 0.10 降为 0.055；
- 复查后胶囊轮廓消失，只保留柔和脸颊提亮。

视觉证据：

- 风格化近景：`captures/capture_1784909055506.png`；
- F9 关闭近景：`captures/capture_1784909068495.png`；
- 分辨率均为 2560×1334；
- 开关差异 804,976 像素；
- 强差异 545,778 像素；
- 最大单通道差 46；
- 差异区域限制在角色头肩范围；
- 数字键 `1` 切回全身机位通过。

回归：

- 莱万汀 Debug Validation Layer 120 帧通过；
- 莱万汀 Release 120 帧通过；
- 公共测试模型 Debug 120 帧通过；
- Debug/Release 构建通过。

结论：

- 数字键 `5` 现在是稳定、可复现的脸部作品集机位。
- S19 同时修复了只有在近景下才可见的 Matcap 硬边，说明后续脸部/头发开发都应同时进行全身与近景 QA。

下一节点：

- S20 盘点 `_HN`、`_P`、KK Ramp 与切线方向，优先尝试头发各向异性风格高光。
- 如果原始 KK 数据无法可靠还原，则转向角色展示地台和背景。

### S20：`_HN` 双法线与 KK 风格化头发高光

目标：使用莱万汀头发材质中已经存在的数据恢复发束方向和各向异性高光，同时保持 S14.1 的实体遮挡修复。

资产审计：

- 检查 `T_actor_laevat_hair_01_HN.png` 与 `T_actor_laevat_hair_01_P.png` 的尺寸、通道均值、标准差和可视分布；
- `_HN` 的 RGB 不满足普通切线空间 Normal Map 的向量分布；
- 母材质字符串与实例参数确认存在 `NormalMap_Base`、`NormalMap_HeighLight`、`Tangent`、`T_Shift`、`KK_Dirction`、`KK_Power`、`KK_Ramp` 与 `KK_Ramp_Strengh`；
- `M_Common_Hair` 使用 `Material Attributes` 输出；UE Python 能定位输出节点，但不能继续遍历 Set Material Attributes 内部输入；
- 综合证据后使用 `RG = Base Normal XY`、`BA = Highlight Normal XY`，没有把 `_HN` 当作普通 RGB 法线强行解码。

数据链路：

- 注入器新增 `afterglowHairDataTexture` 与 `afterglowHairParameters`；
- 重新生成的 GLB 为 63,308,828 bytes；
- 输出统计为 1 个 Hair Data 材质、23 张去重内嵌纹理；
- `AssetMaterial` 增加 Hair Data 像素、尺寸与四分量参数；
- 无 Hair Data 材质使用 2×2 中性 RGBA 回退，启用权重为 0；
- `GpuMaterial` 增加 Hair Data 纹理；
- Descriptor Set Layout 新增 binding 8；
- 每材质组合采样器从 7 个增加到 8 个；
- Material Push Constants 从 64 bytes 扩展为 80 bytes，并保留编译期大小检查。

Shader：

- 新增双通道法线重建函数，并按 Vulkan/glTF 法线方向翻转 Y；
- `RG` 基础法线以 0.45 权重加入头发着色法线；
- `BA` 高光法线用于高光方向修正；
- 使用发束 Bitangent 和 Half Vector 构造 Kajiya–Kay 正弦项；
- Unreal `KK_Power` 映射到 24–96 的安全指数范围；
- `KK_Ramp` 控制窄光带过渡，`KK_Ramp_Strengh` 控制能量；
- 首轮近景调校后收窄阈值、降低总能量，并把高光色拉回暖红发色；
- F9 关闭时 KK 高光归零，基础纹理与常规光照仍可作为同机位对照。

视觉 QA：

- 数字键 `5` 近景可看到从发旋向刘海延伸的发束法线细节；
- KK 高光保持为克制的暖色窄亮带，没有形成白色塑料块或冷色面光；
- 数字键 `1` 全身构图未出现头发 UV 彩噪、轮廓破裂或其他材质误采样；
- 头部、口腔、衣服与手臂遮挡连续，没有重新出现后层结构穿透；
- 风格化近景：`captures/capture_1784910099537.png`；
- F9 关闭近景：`captures/capture_1784910014799.png`；
- 全身节点图：`captures/capture_1784910114525.png`。

回归：

- 莱万汀 Debug Validation Layer 120 帧通过；
- 莱万汀 Release 120 帧通过；
- 公共测试模型 Debug 120 帧通过，中性 Hair Data 回退正常；
- Debug/Release 构建通过；
- Descriptor、Push Constants 与资源销毁路径没有 Validation warning/error。

结论：

- S20 已把头发从“只有几何法线的连续表面”推进为带发束方向的专用材质路径；
- 实现只对包含 `_HN` 和启用参数的头发材质生效；
- 这是一条基于真实资产证据的兼容还原，不是对 Unreal 母材质图的逐节点复制；
- Alpha、混合和深度路径未改变，角色不透明遮挡修复继续有效。

下一节点：

- S21 进入作品集展示节点：优先实现简单地台、渐变背景与主/辅/轮廓光构图；
- 保持数字键 `1` 全身和 `5` 近景作为固定验收机位；
- 展示场景稳定后，再决定导出 KK Curve Atlas、增加屏幕空间内轮廓，或开始骨骼动画。

### S21：程序化地台、渐变背景与三点式展示光

目标：把已经可用的角色材质节点整理成可直接用于秋招作品集截图的展示构图，同时保持 S20 的角色资产路径不变。

地台实现：

- 在 `loadGltfAsset` 完成 bounds 后，由 `appendShowcasePlatform` 追加运行时几何；
- 使用 96 段顶面扇形和独立侧壁顶点；
- 最终增加 289 vertices、864 indices、1 primitive、1 material；
- 半径从首版 extent × 0.62 调整为 × 0.40，避免正面构图像角色站在半颗球体上；
- 顶面安全间隙从 extent × 0.018 收紧到 × 0.004，使靴底与台面视觉接触；
- 厚度最终为 extent × 0.05；
- 地台不参与重新计算 bounds，原角色缩放、相机和描边尺度保持不变；
- Material Push Constants 原 padding 字段改为 `showcasePlatform`，结构仍为 80 bytes；
- Shader 使用地台 UV 生成中心接触压暗与边缘环带。

背景管线：

- 新增 fullscreen-triangle `background.vert/background.frag`；
- 不需要 Vertex Buffer、Descriptor Set 或额外 Pipeline Layout；
- 背景管线关闭 Depth Test/Write，在角色与地台之前绘制；
- 初版线性背景在交换链显示后偏亮，降低为：
  - 暗端 `(0.010, 0.016, 0.028)`；
  - 亮端 `(0.038, 0.060, 0.080)`；
  - 暖紫 Halo `(0.038, 0.016, 0.028)`；
- Vignette 边缘权重由 0.66 收紧到 0.60；
- 最终主体重新成为画面最亮区域，深色服装和机械轮廓不再融入灰背景。

三点式光照：

- Key：暖色主光 `(0.48, 0.82, 0.32)`；
- Fill：冷蓝辅光 `(-0.62, 0.34, -0.48)`，强度 0.13；
- Rim：基于 `pow(1 - N·V, 3.2)` 与背侧方向遮罩，强度 0.12；
- 主光继续驱动 toon diffuse band 与直接高光；
- Fill 补充暗侧结构，不改变 toon band 阈值；
- Rim 受 F9 控制，不作用于地台。

视觉 QA：

- 数字键 `1` 全身图中角色、地台与背景完整，四周保留作品集排版负空间；
- 地台直径与角色机械裙甲宽度接近，前后边缘不再占满屏幕；
- 靴底落在台面中心，未发现明显悬空或穿台；
- 数字键 `5` 近景中头角、头发、脸部与肩甲均能从背景中分离；
- 深色最终全身截图：`captures/capture_1784910873037.png`；
- 深色最终近景截图：`captures/capture_1784910897443.png`；
- 中间亮背景调校稿保留为 `captures/capture_1784910747375.png`，不作为最终节点图；
- 所有最终截图均由 Vulkan 原生交换链读回生成，不包含 Windows 鼠标或通知层。

回归：

- 莱万汀 Debug Validation Layer 120 帧通过，无 warning/error；
- 莱万汀 Release 120 帧通过；
- 公共测试模型 Debug 120 帧通过；
- Debug/Release C++ 与六个 shader 构建通过；
- Swapchain 重建时 Background Pipeline 一并销毁和重建；
- 公共模型的地台材质与中性纹理回退正常。

结论：

- S21 已形成第一个可直接截图的作品集展示节点；
- 展示节点完全由 renderer 生成，不扩散或改写私有角色资产；
- 当前接触阴影为程序化地台压暗，不是 Shadow Map，适合作为静态作品集节点但不是最终动态光照方案；
- S14.1 的实体遮挡修复、S19 近景与 S20 头发数据路径均保持有效。

下一节点：

- S22 优先实现可交互的灯光/背景展示预设，并增加一个更有设计感的终末地风格构图模式；
- 随后在 Shadow Map、屏幕空间内轮廓与骨骼动画之间选择下一个技术节点；
- 秋招作品集侧可先使用 S21 全身图和近景图，补充一张 F9 开关或多角度对比。

### S22：可交互展示预设

目标：在不重新导出角色资产的前提下，把 S21 的单一展示场景扩展为可复现的作品集构图、主题构图和材质检查环境。

交互与数据：

- `F1`：Afterglow Gallery；
- `F2`：Endfield Industrial；
- `F3`：Neutral Material Check；
- Camera UBO 新增 `showcaseParameters`：
  - X：预设编号；
  - Y：主光强度；
  - Z：辅光强度；
  - W：轮廓光强度；
- 三套参数依次为 `(0, 1.00, 0.13, 0.12)`、`(1, 0.92, 0.16, 0.16)`、`(2, 0.95, 0.08, 0.05)`；
- 背景管线绑定当前帧已有 Descriptor Set，以便读取 Camera UBO；未新增贴图、描述符类型或资产依赖。

三套视觉路径：

- Afterglow Gallery 保留 S21 的深蓝紫渐变、暖紫 Halo、暖主光和冷辅光；
- Endfield Industrial 使用深青渐变、低对比度规则网格和右侧琥珀色斜向警戒条纹；
- 工业预设首轮 QA 中网格和条纹过亮，最终将网格能量压低约四倍，并把条纹限制到更靠右的区域；
- Neutral Material Check 使用中性灰渐变、白色主光和低饱和辅光，避免主题颜色干扰材质判断；
- 地台按预设改变轻微色调，仍保留 S21 的中心接触压暗与边缘环带；
- Key、Fill、Rim 的颜色和强度均由当前预设响应，toon band、头发 KK、Matcap 和 F9 总开关保持兼容。

视觉 QA：

- 三套全身固定机位均保留角色、地台和背景的完整构图；
- F2 网格和警戒条纹不会覆盖角色边缘或抢夺面部焦点；
- F3 没有出现皮肤、白色肩甲或金属区域过曝；
- F2 近景中头发、眼睛、口腔、肩甲和衣物前后遮挡连续；
- 未观察到头部后层口腔结构、手臂或其他内部几何穿过前层表面，角色整体不透明；
- F1 全身：`captures/capture_1785246779291.png`；
- F2 全身：`captures/capture_1785246788011.png`；
- F3 全身：`captures/capture_1785246798074.png`；
- F2 近景：`captures/capture_1785246808707.png`；
- 截图均由 Vulkan 交换链原生读回，不包含 Windows 鼠标或通知层。

回归：

- Debug 与 Release 最终构建通过；
- 莱万汀 Debug Validation Layer 120 帧通过，无 warning/error；
- 莱万汀 Release 120 帧通过；
- 公共测试模型 Debug Validation Layer 120 帧通过；
- 莱万汀统计保持为 81,487 vertices、284,673 indices、14 primitives、15 materials；
- 公共模型统计保持为 337 vertices、900 indices、3 primitives、4 materials；
- 预设切换不改变 Alpha、混合、深度写入或实体遮挡路径。

结论：

- S22 提供了可直接用于作品集的默认、主题和诊断三种展示环境；
- F2 是当前秋招展示的推荐主画面，F1 可作为较克制的备选，F3 用于技术拆解或材质对照；
- 本节点只扩展运行时 UBO 与 shader 响应，没有扩散或改写私有角色资产。

下一节点：

- S23 实现方向光 Shadow Map，并使用 3×3 PCF 降低锯齿；
- 真实阴影优先覆盖角色投射到地台的接触与轮廓关系；
- 保留现有程序化中心压暗作为禁用 Shadow Map 时的稳定回退；
- 继续使用数字键 `1` 全身和 `5` 近景完成遮挡、阴影偏移与 Peter-panning QA。

### S23：方向光 Shadow Map 与 3×3 PCF

目标：把 S21/S22 的程序化接触压暗升级为真实的角色投影和自阴影，同时保持现有 Forward + Toon、透明排序、实体遮挡和作品集预设稳定。

资源与同步：

- 新增固定 2048×2048 Shadow Map；
- 深度图使用设备本地内存，同时具备 Depth Attachment 与 Sampled usage；
- 新增独立 Shadow Render Pass、Framebuffer 和 Clamp-to-Border 采样器；
- Shadow Render Pass 清除深度、保存结果，并转换到只读采样布局；
- External → Shadow dependency 同步上一帧 Fragment Shader 读取与本帧深度写入；
- Shadow → External dependency 同步深度写入与随后主材质 Fragment Shader 读取；
- 该同步允许双帧在途继续共用一张 Shadow Map，而不引入额外 CPU 等待。

光源矩阵与管线：

- Camera UBO 新增 64-byte `lightModelViewProjection`；
- 光方向与现有主光保持一致：`normalize(0.48, 0.82, 0.32)`；
- 光源位于目标沿光方向 4.5 单位处；
- 使用 `[-1.9, 1.9]` 的 Vulkan Y 翻转正交投影和 0.1–8.0 深度范围；
- 新增 `shadow.vert` 与 `shadow.frag`；
- Shadow Pipeline 使用 Position 与 UV 两个顶点属性；
- 关闭面剔除，以兼容 S14.1 中确认过的混合三角形绕序；
- Raster Depth Bias 为 constant 1.25、slope 1.75；
- 程序化地台不作为 caster，避免自阴影；所有角色 primitive 均参与投影。

Alpha 与 PCF：

- Shadow Fragment Shader 采样每材质 Base Color；
- MASK 继续使用材质原始 `alphaCutoff`；
- BLEND 使用 0.35 的阴影投射阈值，避免眉毛、发影等薄层投出完整矩形；
- 主材质新增 Descriptor binding 9；
- 手动 3×3 PCF 使用 9 次相邻深度比较；
- 接收偏移根据 `1 - N·L` 在 0.00025–0.0011 之间变化；
- Shadow Map 只削弱主光漫反射、直接高光和 KK 头发高光；
- 主光阴影最大混合权重 0.72，环境光、辅光和轮廓光仍保留可读性。

地台可读性调校：

- 第一轮实现已产生正确角色自阴影，但正面机位中地台投影被主光/相机同侧构图和原程序化中心压暗掩盖；
- 将程序化中心压暗从 0.48 减弱到 0.72；
- 地台接收面在 Shadow Map 覆盖处额外把环境光最低降到 44%；
- 正面机位保持克制，避免脚下形成生硬黑斑；
- 左侧机位 `4` 能清楚显示裙甲、机械外装和长条结构投向地台的轮廓。

视觉 QA：

- 左侧全身机位能识别真实投影方向、机械轮廓和 3×3 PCF 软化边缘；
- 脚底阴影与接触点连续，没有明显悬浮或 Peter-panning；
- F2 正面构图保持角色为视觉中心，阴影不会盖过工业网格背景；
- F2 近景中脸部、刘海、头角和肩甲没有条纹状 shadow acne；
- 近景未出现脸部整体压黑、口腔穿透、手臂穿衣或新的 Alpha 回归；
- 侧面投影证据：`captures/capture_1785248100701.png`；
- F2 正面节点图：`captures/capture_1785248137040.png`；
- F2 近景证据：`captures/capture_1785248160325.png`；
- 三张图均由 Vulkan 交换链原生读回生成。

回归：

- Debug 与 Release 最终构建通过，新增八个 shader 均成功编译；
- 莱万汀 Debug Validation Layer 120 帧通过，无 warning/error；
- 莱万汀 Release 120 帧通过；
- 公共测试模型 Debug Validation Layer 120 帧通过；
- 最大化窗口触发 Swapchain 重建后 Shadow Pipeline 正常重建；
- Shadow Render Pass、Framebuffer、Sampler、Image View、Image 和 Device Memory 均进入明确销毁路径。

结论：

- S23 已形成真实方向光深度通道，而不是基于屏幕或 UV 的伪阴影；
- 投影与主光使用同一方向，材质 Alpha 和角色双面表面得到兼容处理；
- 当前单张 2048² Shadow Map 适合单角色作品集展示，但不是开放世界级阴影方案。

下一节点：

- S24 优先实现屏幕空间内轮廓/深度法线边缘增强，使机械装甲内部结构在正面机位更清晰；
- 保留现有 inverted-hull 外描边，新增效果只补充角色内部结构线；
- 完成后再进入骨骼蒙皮与基础 Idle 动画，避免动画放大当前内部轮廓不足的问题。

### S24：屏幕空间深度/法线内部轮廓

目标：在保留 S13 inverted-hull 外描边的前提下，通过真实的屏幕空间深度与法线邻域检测，补充肩甲、服装接缝和机械结构的内部轮廓。

附件与 Render Pass：

- 每张交换链图像新增 `VK_FORMAT_R8G8B8A8_UNORM` 法线附件；
- 主场景深度图增加 `VK_IMAGE_USAGE_SAMPLED_BIT`，深度格式选择同时检查采样支持；
- 主场景 Subpass 同时写入交换链颜色和法线两个 Color Attachment；
- 法线附件结束布局为 `SHADER_READ_ONLY_OPTIMAL`，深度附件结束布局为 `DEPTH_STENCIL_READ_ONLY_OPTIMAL`；
- 新增第二个 Post-process Render Pass，以 `LOAD` 保留场景颜色，并在结束时转换到 Present；
- 新增 Post-process Descriptor Set Layout、Pool、每交换链图像 Descriptor Set、最近点 Clamp-to-Edge Sampler、Pipeline Layout、Pipeline 与 Framebuffer；
- Swapchain 重建和清理路径覆盖上述全部资源。

Shader：

- 新增无 Vertex Buffer 的 fullscreen-triangle `inner_outline.vert`；
- `mesh.frag` 在 location 1 输出编码几何法线与内部轮廓参与权重；
- `inner_outline.frag` 读取法线和深度，检查八个相邻像素；
- 深度差放大系数为 1400，深度阈值为 0.18；
- 法线差使用 `1 - dot(N0, N1)`，阈值为 0.20；
- 输出颜色为深蓝黑 `(0.008, 0.013, 0.022)`，默认强度由首轮 0.48 收敛到 0.40；
- 背景/地台参与度为 0，普通角色材质为 1；
- Hair Data 和 Matcap 材质参与度为 0.22，邻域差异再乘中心与相邻像素的较小参与度；
- `F10` 切换内部轮廓开关，关闭时只把 Push Constant 强度设为 0。

Validation 修复：

- 首轮公开模型 Validation 发现两个 Color Attachment 使用不同 Blend State，而设备未启用 `independentBlend`；
- 修复为两个附件使用相同的 Blend Attachment State；
- 法线附件的 Alpha 仍由 shader 正常写入，不依赖独立颜色混合配置；
- 修复后私有 Debug 120 帧立即通过，无 warning/error。

视觉调校：

- 首轮强度 0.48 能增强肩甲，但近景会逐条描出头发网格法线岛，形成过密线条；
- 引入材质参与权重并把强度降至 0.40 后，脸部恢复干净，头发只保留少量发束分界；
- 肩甲外层、内层板件、领口与胸前机械层次仍明显强于关闭状态；
- 屏幕空间 pass 不重新描绘背景和地台，也不会替代已有角色外轮廓；
- 首轮过密线条参考：`captures/capture_1785249959349.png`；
- 最终 F10 开启近景：`captures/capture_1785250216981.png`；
- 最终 F10 关闭对照：`captures/capture_1785250231178.png`；
- 对照图均由 Vulkan 交换链原生读回生成。

回归：

- Debug 与 Release 最终构建通过，`inner_outline.vert/frag` 和修改后的 `mesh.frag` 均成功编译；
- 莱万汀 Debug Validation Layer 120 帧通过，无 warning/error；
- 莱万汀 Release 120 帧通过；
- 公共测试模型 Debug Validation Layer 120 帧通过，无 warning/error；
- 莱万汀统计保持为 81,487 vertices、284,673 indices、14 primitives、15 materials；
- 公共模型统计保持为 337 vertices、900 indices、3 primitives、4 materials；
- 最大化窗口触发 Swapchain 重建后，法线/深度采样和 Post-process Pipeline 正常工作。

结论：

- S24 已形成真实的屏幕空间内部轮廓路径，而不是材质 Fragment Shader 内的导数近似；
- 外轮廓、内部轮廓、方向光阴影和材质响应现在是相互独立的四条路径，可分别调试；
- 材质参与权重解决了角色近景中“结构增强”和“头发/脸部过描”之间的冲突；
- 当前阈值针对单角色作品集画面调校，尚未提供运行时连续参数 UI。

下一节点：

- S25 开始骨骼蒙皮基础：读取 `JOINTS_0`、`WEIGHTS_0`、skin joints 与 inverse bind matrices；
- 先在静态 bind pose 下验证 GPU joint palette 和顶点变换，再接入 animation sampler/channel；
- 保留静态资产和公共模型 fallback，避免蒙皮功能破坏当前作品集节点。

### S25：Bind Pose GPU 蒙皮基础

目标：在进入动画播放前，建立可验证的 glTF Skin、Joint Palette 和四权重 GPU 顶点蒙皮路径，并保持 S24 静态作品集节点完全兼容。

资产审计：

- `laevat_static_material.glb` 只有 1 node、1 mesh、0 skins、0 animations；
- 顶点属性只有 POSITION、NORMAL、TANGENT、TEXCOORD_0/1；
- 原静态导出脚本明确设置了 `export_vertex_skin_weights = False`；
- 因此不能从旧 GLB 恢复真实骨骼，必须从 Unreal Skeletal Mesh 重新导出；
- 新导出不覆盖旧文件，使用独立的 `assets_private/laevat_skinned` 目录。

Unreal 导出：

- 新增 `tools/unreal_export_laevat_skinned.py`；
- Unreal 5.7 Python Commandlet 以 Null RHI 只读导出 Skeletal Mesh；
- 开启 Vertex Skin Weights，暂时关闭 Morph Target 和 Animation Sequence；
- 新原始 GLB 为 6,142,592 bytes；
- 审计结果为 469 nodes、1 skin、468 joints、105 accessors；
- 顶点属性包含 `JOINTS_0` 与 `WEIGHTS_0`；
- `inject_gltf_textures.js` 增加可选输入/输出路径，同时保持无参数静态流程兼容；
- 最终材质版骨骼 GLB 为 64,324,908 bytes，包含原 23 张纹理。

Loader：

- `AssetVertex` 增加 `uvec4 joints` 和 `vec4 weights`；
- 静态顶点默认使用 joints `(0,0,0,0)`、weights `(1,0,0,0)`；
- JOINTS 支持 U8/U16/U32；
- WEIGHTS 支持 float 和 normalized U8/U16；
- 对四权重重新归一化并处理零权重回退；
- 建立节点 Parent 表并递归计算全局 Transform；
- 读取 MAT4 Float inverse bind accessor；
- Bind Pose Joint Palette 使用 `jointWorld × inverseBindMatrix`；
- Skinned Primitive 保留原始顶点空间，静态 Primitive 继续烘焙 Node Transform；
- 当前明确限制为每资产一个 skin，非法 Joint Index 会在 Loader 阶段报错；
- `LoadedAsset` 保存 Joint Matrix 数组和 `hasSkin` 状态。

GPU 路径：

- Vertex Layout 从 4 个属性扩展为 6 个属性；
- location 4 为 `R32G32B32A32_UINT` Joint Indices；
- location 5 为 `R32G32B32A32_SFLOAT` Joint Weights；
- 每帧创建一个 Host-visible、Host-coherent Joint Storage Buffer；
- Descriptor binding 10 为 `VK_DESCRIPTOR_TYPE_STORAGE_BUFFER`，仅 Vertex Stage 可见；
- `mesh.vert` 蒙皮 Position、Normal 和 Tangent；
- `outline.vert` 使用蒙皮后的 Position/Normal 生成 inverted hull；
- `shadow.vert` 使用同一蒙皮 Position 写入 Light Clip Space；
- 无 Skin 的资产绑定单张 Identity Matrix，所有资产共用相同 Pipeline 和 Descriptor Layout。

Validation 调整：

- 首轮测试发现 Outline Pipeline 仍只声明两个 Vertex Attribute，缺少新增 location 4/5；
- 补齐后能创建 Pipeline，但把完整六属性传给 Outline Shader 会产生未使用 location 2/3 警告；
- 最终为 Outline 使用 0/1/4/5 精确属性子集，为 Shadow 使用 0/3/4/5 子集；
- 最终 Debug Validation 无 warning/error。

视觉 QA：

- Bind Pose 全身比例、位置和 S24 静态版本一致；
- 脸部、刘海、头角、肩甲、衣物与手臂前后遮挡连续；
- Base Color、Normal、Metallic/Roughness、Matcap、Hair Data 与 Style Mask 保持有效；
- 外描边贴合蒙皮后几何；
- 屏幕空间内部轮廓没有新增噪点；
- Shadow Map 与角色几何一致，没有静态/蒙皮位置分离；
- 正面证据：`captures/capture_1785253111387.png`；
- 近景证据：`captures/capture_1785253132877.png`。

回归：

- Debug 与 Release 构建通过；
- 骨骼版莱万汀 Debug Validation 120 帧通过，无 warning/error；
- 骨骼版莱万汀 Release 120 帧通过；
- 原静态莱万汀 Debug Validation 120 帧通过；
- 公共静态模型 Debug Validation 120 帧通过；
- 静态 fallback 日志显示 1 joint matrix，骨骼版显示 468 joint matrices。

结论：

- S25 已完成真实 glTF Skin 数据到 Vulkan Vertex Shader 的端到端路径；
- 当前画面仍是 Bind Pose，但顶点已经实际通过 Joint Storage Buffer 计算；
- 静态与骨骼资产不需要不同的 Pipeline，降低了 S26 动画接入的回归风险；
- 旧静态作品集资产继续保留，可随时作为视觉和性能基线。

下一节点：

- S26 解析 glTF animation sampler/channel；
- 保存节点 Local TRS、Parent、Joint Node 映射和 inverse bind matrices；
- 实现 Translation/Scale Linear、Rotation Slerp 与循环时间轴；
- 每帧重建 Joint Palette 并写入当前帧 Storage Buffer；
- 优先接入一个可复现的 Idle Animation，再进行蒙皮动态视觉 QA。

### S26：glTF 动画运行时与程序化 Idle

目标：在 S25 的真实 GPU 蒙皮基础上接通 glTF 动画时间轴，使节点局部
TRS、骨骼层级、Joint Palette、主材质、外描边与阴影在每帧保持同步。

资产决策：

- Unreal 5.7 Commandlet 扫描了工程中的 2 个 `AnimSequence`；
- 没有动画使用莱万汀 Skeleton，结果记录在
  `assets_private/laevat_skinned/compatible_animations.json`；
- 不对未知骨架执行自动重定向，避免把错误姿态误判为运行时问题；
- 新增 `tools/inject_gltf_idle_animation.js`，向 S25 骨骼材质 GLB 注入
  一段确定性的四秒循环 Idle；
- 动画只轻微旋转 `Bip001_Spine2` 与 `Bip001_Head`，用于证明层级传播、
  四元数插值和动态蒙皮，不修改 Unreal 源资产；
- 新私有测试文件 `laevat_idle_material.glb` 为 64,326,176 bytes，
  动画名 `Afterglow_ProceduralIdle`，包含 17 个关键时刻与 2 条旋转通道。

Loader：

- `LoadedAsset` 现在保存节点 Parent、Local TRS、Local Matrix、Joint Node
  映射和 inverse bind matrices；
- 解析 glTF animation sampler 与 channel；
- 支持 Translation、Rotation、Scale 三种 Target Path；
- 支持 STEP 与 LINEAR；明确拒绝尚未实现的 CUBICSPLINE；
- Translation/Scale 使用分段线性插值；
- Rotation 使用归一化四元数 Slerp，并处理最短弧与近共线回退；
- 时间轴以动画自身起止时间循环，首尾一致的 Idle 不产生跳变；
- 每次采样从原 Bind TRS 开始，随后递归重建全部节点 World Transform；
- 最终 Joint Palette 仍为 `jointWorld × inverseBindMatrix`。

Runtime：

- 默认播放第一个 glTF 动画；
- 每帧只更新已等待 Fence 的当前 in-flight Joint Storage Buffer；
- `F4` 暂停/继续动画，`F11` 归零并恢复播放；
- 启动日志输出动画数量、名称和循环时长；
- 无动画骨骼资产继续保持 Bind Pose；
- 无 Skin 静态资产继续使用单张 Identity Joint Matrix；
- 主材质、inverted-hull 外描边和 Shadow Pass 继续共用同一动态蒙皮结果。

验证：

- Debug 构建通过；
- Release 构建与动画莱万汀 120 帧运行通过；
- 动画莱万汀 Debug Validation 300 帧通过，日志显示 1 个动画、
  468 joint matrices 和 4 秒循环；
- 无动画骨骼莱万汀 Debug Validation 120 帧通过；
- 原静态莱万汀 Debug Validation 120 帧通过；
- 公共静态模型 Debug Validation 120 帧通过；
- 动画窗口额外运行 10,000 帧，并完成自动旋转及固定正面视图 QA；
- 未出现 Validation warning/error、骨骼爆炸、网格撕裂、阴影分离或
  外描边脱离；
- 角色衣服、手臂、头部与内部结构的前后遮挡关系在动态姿态中保持稳定，
  未重新出现半透明观感。

当前限制：

- 当前只自动播放第一个动画，不含动画选择 UI；
- 不支持多动画混合、Cross-fade、Additive Animation 或 Root Motion；
- 不支持 CUBICSPLINE 与 Morph Target 动画；
- 程序化 Idle 的幅度有意保持很小，它是技术验收资产，不是最终角色表演。

下一节点：

- S27 优先加入动画选择与时间轴诊断信息，并建立可录制的作品集动态镜头；
- 随后选择正式 Idle 来源：制作项目自有动画、取得兼容动画，或实现明确的
  Skeleton Retarget 流程；
- 在正式动画确定前，保留当前程序化 Idle 作为稳定自动回归资产。

### S27：动画诊断与作品集慢速环绕镜头

目标：把 S26 的底层动画能力整理成可稳定操作、可现场演示、可直接录制的
作品集工作流。

动画选择与诊断：

- 新增循环选择上一/下一动画；
- 切换动画时索引循环、时间归零并恢复播放；
- 新增时间轴状态输出，包含 `[当前索引/总数]`、动画名称、当前播放时间、
  总时长与 Playing/Paused 状态；
- 只有一个动画时仍执行完整循环选择，用于验证通用选择路径；
- 初版 `P/N/T` 字母键受当前中文输入环境影响，没有稳定进入 GLFW 回调；
- 第二版 `PageUp/PageDown/Home` 在当前窗口输入链路中同样不稳定；
- 最终使用数字键 `7/8/9`，实际日志已验证三者均可靠触发；
- `7` 为上一动画、`8` 为下一动画、`9` 为时间轴状态。

作品集镜头：

- 数字键 `6` 一键进入 Portfolio Orbit；
- 初始角色角度为 45 度；
- 自动启用 Endfield Industrial 展示预设、风格化光照和内部轮廓；
- 动画时间归零并恢复播放；
- 自动旋转速度从普通预览的 `0.65 rad/s` 降为 `0.16 rad/s`，
  完整一圈约 39.3 秒；
- `R` 恢复普通预览旋转及 `0.65 rad/s`；
- 首轮相机 `(2.20, 1.72, 2.52)`、目标 `(0, 0.18, 0)` 构图过紧，
  靴子贴近下边缘；
- 最终相机调整为 `(2.32, 1.80, 2.66)`，目标下移至 `(0, 0.05, 0)`；
- 最终画面保留头顶和脚底安全边距，同时让角色比普通全身预设更突出。

动态 QA：

- `6` 号镜头从正面、侧面和背面连续观察通过；
- 角色全身、肩甲、武器、衣摆和靴子在环绕过程中保持画面内可读；
- 工业网格背景、平台、主光、阴影、外描边和内部轮廓同步正常；
- 未出现骨骼爆炸、动态网格撕裂、阴影脱离或半透明/内部结构穿透；
- `F4` 暂停日志正常；
- 最终数字键日志依次输出：
  `3.51955 / 4 s`、上一动画归零、下一动画归零；
- 测试窗口正常关闭，最终一轮运行 23,069 帧。

最终回归：

- Debug 与 Release 构建通过；
- 动画莱万汀 Debug Validation 180 帧通过；
- 动画莱万汀 Release 120 帧通过；
- 无动画骨骼莱万汀 Debug Validation 120 帧通过；
- 公共静态模型 Debug Validation 120 帧通过；
- 静态模型继续显示 1 张 Identity Joint Matrix，骨骼模型保持 468 张；
- 所有测试均正常退出，没有 Validation warning/error。

结论：

- S27 已形成“一键展示 + 动画诊断 + 动画选择”的作品集运行路径；
- 当前没有为了镜头展示修改角色 GLB，静态和动态资产边界保持不变；
- 数字键方案比字母和导航键更适合当前中文 Windows 演示环境；
- `6` 号镜头现在可作为后续原生视频录制和秋招作品集素材的默认入口。

下一节点：

- S28 建立无损或高质量的视频捕获流程，并输出第一段可用于作品集剪辑的
  固定分辨率动态样片；
- 同时记录 GPU、分辨率、帧数与动画/镜头配置，形成可复现的性能展示数据；
- 视频节点完成后，再决定正式 Idle 动画制作或 Skeleton Retarget 的优先级。

### S28：确定性离线捕获与首段 1080p60 样片

目标：生成不受桌面录屏、VSync、窗口遮挡或实时渲染速度影响的作品集视频源，
并保存足够的环境信息以复现结果。

CLI 与安全约束：

- `VulkanRunOptions` 统一保存资产、Smoke、窗口、作品集和捕获参数；
- 新增 `--width`、`--height`、`--portfolio`、`--capture-dir`、
  `--capture-frames` 与 `--capture-fps`；
- 分辨率限制为 64×64 至 7680×4320；
- 捕获帧率限制为 1–240 fps；
- `--capture-dir` 与 `--capture-frames` 必须共同使用；
- 捕获目录必须不存在或为空；
- 非空目录直接失败，实测不会删除或覆盖现有 PNG；
- 捕获窗口不可 Resize；
- 实际 Swapchain Extent 不等于请求尺寸时直接失败。

固定时间步：

- 捕获第一帧使用零增量，精确表示动画与镜头的初始状态；
- 后续每帧使用 `1 / captureFps`；
- 动画时间、角色环绕和 GPU Joint Palette 使用同一固定增量；
- 普通交互和 Smoke 模式继续使用 `glfwGetTime()`；
- 因此 PNG 压缩或 GPU/CPU 渲染耗时不会改变捕获内容。

GPU 捕获：

- 每个捕获帧创建 Host-visible Transfer Destination Buffer；
- Scene、内部轮廓和后处理完成后，把最终 Swapchain Image 复制到 Buffer；
- 等待当前 Frame Fence 后执行 RGBA/BGRA 归一化；
- 使用 stb 写出无损 RGBA PNG；
- 文件名固定为 `frame_%06d.png`；
- 仅在第 1 帧、每秒末尾和最终帧输出进度，避免 240 行日志；
- 手动 F12 截图路径保持兼容。

Manifest：

- 序列结束后写出 `capture_manifest.json`；
- 记录格式版本、资产路径、GPU、宽高、fps、捕获/渲染帧数、时长、
  Portfolio 状态、Animation Index/Name 和 Frame Pattern；
- 正式样片记录为 RTX 4060 Laptop GPU、1920×1080、60 fps、240 帧、
  4 秒和 `Afterglow_ProceduralIdle`。

确定性验证：

- 先生成两组 1280×720、60 fps、6 帧 Debug Validation 探针；
- 两组从 `frame_000000.png` 至 `frame_000005.png` 的 SHA-256 全部逐帧一致；
- 探针 Manifest 为 6 captured/rendered frames 和 0.1 秒；
- 非空探针目录复用测试按设计返回 Fatal Error；
- 正式 Release 捕获精确生成 240 张 1920×1080 PNG；
- 序列总大小约 216.9 MiB；
- 正式捕获正常输出 1/60/120/180/240 帧进度并自动退出。

视频编码：

- 新增 `tools/encode_capture.ps1`；
- 脚本校验 Manifest 和第一帧，并拒绝覆盖现有输出；
- 使用 FFmpeg `libx264`、High Profile、CRF 15、Slow、YUV420p；
- 写入 BT.709 Color Range/Space/Transfer/Primaries；
- 启用 `+faststart`，不生成音轨；
- 本轮使用 FFmpeg 8.1.2 Essentials 便携构建；
- 下载包 SHA-256 与发布方提供的
  `db580001caa24ac104c8cb856cd113a87b0a443f7bdf47d8c12b1d740584a2ec`
  一致后才解压运行；
- `ffprobe` 验证最终 MP4：H.264 High、1920×1080、YUV420p、
  BT.709、60/1 fps、240 frames、4.000000 seconds；
- 最终文件 `captures/Afterglow_S28_Portfolio_1080p60.mp4`，
  大小 3,619,144 bytes。
- 最终 MP4 SHA-256：
  `7E72167D2A03AFAE3DFDE9A333A8E7B6931ED31DAE623076BF7974DEE067F601`；
- 对同一输出路径再次运行编码器时，脚本按设计拒绝覆盖。

视觉 QA：

- 原始检查帧为 0、120、239；
- 首帧完整正面，中间帧进入轻微三分之二角度，末帧约 18 度侧面；
- 角色头顶、肩甲、双侧机械结构、衣摆和靴子均保持安全边距；
- 平台底部裁切稳定；
- 没有窗口边框、鼠标或桌面录屏叠加；
- 没有网格撕裂、阴影脱离、描边错位、内部结构穿透或半透明回归。

最终回归：

- Debug 与 Release 构建通过；
- 两次 Debug 捕获均启用 Validation Layer 并正常退出；
- 正式 Release 捕获正常完成；
- 公共静态模型 Debug Validation 120 帧通过；
- 普通 `--smoke-frames` 与手动 F12 截图接口保持兼容。

结论：

- S28 已建立从固定时间步 Vulkan 渲染、无损帧序列、Manifest 到高质量 MP4
  的完整作品集捕获路径；
- 当前样片足以作为秋招作品集的技术预览素材；
- 原始 PNG 保留后可以重复编码不同码率，而不需要重新渲染；
- 捕获输出位于被忽略的 `captures/`，不会意外提交第三方角色画面。

下一节点：

- S29 增加 GPU 时间戳查询与统计窗口，输出可复现的平均帧时和 Pass 分布；
- 为作品集准备一份技术拆解画面：Beauty、Shadow、Normal、Internal Outline
  与 Stylized On/Off；
- 再根据展示需要决定延长镜头、增加淡入淡出或制作正式 Idle。

### S28.1：20 秒 1080p60 作品集视频

目标：按用户要求把四秒技术预览延长为可用于作品集剪辑、并能覆盖角色背面
与多次动画循环的 20 秒长镜头。

捕获与编码：

- 继续使用 S28 的固定时间步和 S27 Portfolio Orbit；
- Release 捕获精确生成 1200 张 1920×1080 PNG，帧号 0–1199 连续；
- 60 fps 对应 20.000000 秒，原始序列约 1.033 GiB；
- 序列目录为 `captures/s28_portfolio_1080p60_20s`；
- 成片为 `captures/Afterglow_S28_Portfolio_1080p60_20s.mp4`；
- MP4 为 H.264 High、YUV420p、BT.709、60 fps、无音轨；
- 文件大小为 16,485,087 bytes；
- SHA-256 为
  `5305A559FDC425FB2E2A3CD65C947291A6B251CC1CDB68FA015B28268BC40F29`。

视觉 QA：

- 检查第 0、600、1199 帧，覆盖正面、侧面和接近背面的视角；
- 20 秒镜头约旋转 183°，四秒 Idle 完成五次循环；
- 角色始终位于构图安全区，平台底部裁切稳定；
- 未发现骨骼撕裂、半透明回归、衣服内结构穿透、阴影脱离或描边错位。

结论：

- 20 秒成片在保持确定性的同时显著增加了视角和动画覆盖；
- 该版本可直接进入作品集剪辑，也可从无损序列重新编码其他码率版本；
- 长镜头同时承担了蒙皮、深度遮挡和多 Pass 同步的持续运行回归。

### S29：Vulkan GPU Timestamp Query 与 Pass 分布

目标：用 Vulkan 原生 GPU 时间戳量化当前 Forward 路径的三个主要阶段，
让性能结论可以复现，并为下一步优化和作品集技术拆解提供依据。

实现：

- 新增 `--gpu-timing` 和 `--gpu-timing-output <json>`；
- 指定 JSON 输出会自动启用 GPU Timing；
- 已存在的 JSON 路径会被拒绝，避免覆盖旧性能证据；
- 根据物理设备属性读取 `timestampPeriod = 1 ns`；
- 当前图形队列族提供 64-bit timestamp valid bits；
- 为两个 in-flight frame 分别创建含四个槽位的 `VkQueryPool`；
- 每次录制命令缓冲时先重置对应 Query Pool；
- 四个时间戳依次位于渲染开始、Shadow 结束、Main Scene 结束和
  Internal Outline 结束；
- 等待对应 Frame Fence 后读取结果，避免阻塞当前 GPU 帧；
- 根据 valid bits 建立差值掩码，兼容时间戳回绕；
- 汇总 Shadow、Main Scene、Internal Outline 的平均耗时，以及三阶段
  合计的平均、最小与最大耗时；
- JSON 保存 GPU、资产、分辨率、样本数、timestamp period 与全部统计值。

Debug Validation 验证：

- 1280×720、120 帧、动态角色；
- Shadow 0.173 ms，占 26.034%；
- Main Scene 0.454 ms，占 68.450%；
- Internal Outline 0.037 ms，占 5.517%；
- 合计平均 0.663 ms，最小 0.624 ms，最大 0.698 ms；
- 报告为 `captures/s29_gpu_timing_debug_1280x720.json`；
- 无 Validation warning/error。

正式 Release 数据：

- RTX 4060 Laptop GPU、1920×1080、Portfolio Orbit、600 个有效样本；
- Shadow 平均 0.189631 ms，占 21.520%；
- Main Scene 平均 0.620023 ms，占 70.363%；
- Internal Outline 平均 0.071526 ms，占 8.117%；
- 三阶段合计平均 0.881180 ms；
- 合计最小 0.825184 ms，最大 1.184576 ms；
- 报告为 `captures/s29_gpu_timing_release_1080p.json`。

回归：

- Debug 与 Release 构建通过；
- 公共静态模型 Debug Validation 120 帧通过；
- 动态模型 Debug Timing Validation 120 帧通过；
- 动态模型 Release Timing 600 帧通过；
- Query Pool 在 Renderer 清理阶段逐一销毁；
- 未改变普通交互、确定性捕获或 F12 截图路径。

结论：

- 当前 1080p GPU Pass 成本主要集中于 Main Scene，约占 70.4%；
- Shadow Map 约占 21.5%，Internal Outline 约占 8.1%；
- 0.881 ms 只表示三个 GPU 渲染阶段，不包含 Present、CPU、交换链等待、
  捕获回读或 PNG 编码，不能作为端到端 FPS 宣传；
- 下一节点 S30 将输出 Beauty、Shadow、Normal、Internal Outline 和
  Stylized On/Off 技术拆解图，并考虑加入运行时性能覆盖层。

### S30：可复现的渲染技术拆解视图

目标：让作品集能够展示 Renderer 的中间数据和风格化增量，而不只展示最终
Beauty 画面；所有拆解必须来自同一 Vulkan 帧和确定性相机。

接入设计：

- 复用 S24 的全屏后处理和现有 Normal/Depth Attachment；
- Post-process Descriptor Set 从两个采样输入扩展为三个，新增 Shadow Map；
- 不新增重复 Render Pass 或额外角色 Draw Call；
- `PostProcessPushConstants` 的第四个 float 改为诊断模式编号；
- Beauty 模式保持原有透明混合内部描边；
- 诊断模式输出 Alpha 1，完整替换 Beauty，避免中间数据与最终色彩混合。

四种模式：

1. Beauty：原最终构图；
2. World Normal：直接显示编码后的世界空间几何法线；
3. Internal Outline：以深蓝背景和青色边缘隔离显示 Depth/Normal 响应；
4. Shadow Map：全屏显示 2048×2048 光源空间深度。

Shadow Map 可读性迭代：

- 首版使用 `depth^18`，视觉检查发现内部灰阶被压成近乎纯黑；
- 最终改为 `depth^4`，保留白色远平面，并恢复头部、肩甲、裙摆、武器和
  双腿之间的光源空间深度层次；
- 该曲线只用于诊断显示，不修改 PCF 阴影采样或实际光照结果。

控制与 CLI：

- 数字键 `0` 循环 Beauty、World Normal、Internal Outline、Shadow Map；
- `--diagnostic-view beauty|normal|outline|shadow` 可直接选择启动状态；
- `--no-stylized` 对应 F9 的 Conventional 对照；
- `--no-inner-outline` 对应 F10；
- 未知诊断名称在窗口/Vulkan 初始化前返回 Fatal Error 和退出码 1；
- 启动日志打印诊断、Stylized 和 Internal Outline 状态。

确定性证据：

- 五组最终捕获均为 1920×1080、60 fps 时间基准的第零帧；
- 使用同一 `laevat_idle_material.glb`、Portfolio Orbit、模型角度与动画零点；
- `s30_final_beauty_stylized`：972,835 bytes，
  SHA-256 `8FBCA478C47B094B25E6FEC3274FF6DACB6C18DF2817A77AEDCB092E9B5BA211`；
- `s30_final_beauty_conventional`：962,735 bytes，
  SHA-256 `C8DBAB0A3655E0A3FECB3F3515D5928EFC7E83DC8474BDD55996B23E5A64770D`；
- `s30_final_world_normal`：606,324 bytes，
  SHA-256 `99E50D878529ADD902DE43BBD78FAD38B8EBF74C33253BBA89000BD186A79B50`；
- `s30_final_internal_outline`：162,117 bytes，
  SHA-256 `38D57F66B35FFC3949B03B0F4FBAA4952D1C1D647CEC630902A4392C14CDA98C`；
- `s30_final_shadow_map`：146,362 bytes，
  SHA-256 `0A0F7CD35B274398EA23356C693A063AB5E3230800E38B7F31AC7489A15D556B`。

Manifest：

- 新增 `diagnosticView`、`stylizedLighting`、`internalOutline`；
- 五组最终 Manifest 已逐一解析验证，字段与命令完全一致；
- 旧的 S28 Manifest 仍可兼容读取，编码脚本只依赖原有字段；
- 非空目录和已有输出继续拒绝覆盖。

验证：

- Debug 与 Release 编译通过，包括更新后的 GLSL/SPIR-V；
- Beauty、Normal、Outline、Shadow 各完成六帧动态角色 Debug Validation；
- Shadow 模式单独复核退出码为 0；
- 公共静态模型完成 120 帧 Debug Validation；
- 所有 Validation 测试均无 warning/error；
- 五张最终 PNG 完成视觉检查，没有半透明、内部结构穿透、蒙皮撕裂、
  阴影数据错位或诊断视图与角色轮廓分离。

结论：

- S30 已形成可直接用于作品集排版的 Beauty/技术拆解证据；
- 这些输出来自 Renderer 自身，不是后期伪造的示意图；
- World Normal 和 Internal Outline 能解释 S24 的输入与结果；
- Shadow Map 能解释 S23 的光源空间数据；
- Stylized On/Off 能展示风格化层相对于常规材质路径的增量。

下一节点：

- S31 增加轻量运行时 HUD，显示 GPU、分辨率、动画、诊断模式和 S29
  Pass Timing；
- HUD 默认不进入无 UI 的作品集捕获，提供明确开关；
- 同时评估是否需要在 20 秒成片中插入短暂技术拆解段落。

### S31：轻量 Vulkan 运行时技术 HUD

目标：在不引入 ImGui、字体纹理或大型 UI 框架的前提下，把 S29 的性能数据
和 S30 的诊断状态直接显示在 Renderer 画面中，同时保持现有作品集输出默认
无 UI。

文字路径选择：

- vcpkg 的 stb 依赖已提供 `stb_easy_font.h`；
- 它将 ASCII 字符构造为几何四边形，不需要字体 Atlas、Sampler 或 Descriptor；
- 每个四边形在 CPU 端转换为两个三角形；
- GPU 顶点格式为 NDC `vec2` 加 `R8G8B8A8_UNORM` Color，共 12 bytes；
- 非 ASCII 字符会替换为 `?`，GPU/动画名分别限制长度，避免越界和面板过宽。

GPU 资源：

- 两个 in-flight frame 各自拥有一个 Host-visible、Host-coherent HUD Vertex
  Buffer；
- 单帧最多 24,576 个顶点；
- 每帧只重写当前 Fence 已完成的 Buffer；
- HUD 使用独立 Vertex/Fragment Shader 和无 Descriptor 的 Pipeline Layout；
- Draw 位于 Post-process Render Pass 的 Internal Outline 之后；
- 深度测试关闭，使用标准 Src Alpha 混合；
- 半透明深蓝面板和青色左侧标记也由同一顶点缓冲生成。

显示内容：

- GPU 名称与 Framebuffer 分辨率；
- 当前 Diagnostic View、Stylized 和 Internal Outline 状态；
- 动画名、Playhead、Duration、Playing/Paused；
- S29 Shadow、Main Scene、Internal Outline 和 Total 的运行平均值；
- Timing 前两帧显示 `WARMING UP`；
- 未启用 Timestamp 时显示明确的启用提示，不伪造性能数字。

控制与捕获：

- 新增 `--hud`，并自动设置 `gpuTimingEnabled = true`；
- `H` 运行时切换显示；
- HUD 默认关闭；
- 捕获 Manifest 新增 `hudEnabled`；
- 显式 `--hud` 捕获会包含面板，普通捕获保持无 UI。

坐标修复：

- 首张证据中 HUD 位于左下且字符上下翻转；
- 原因是将 OpenGL 风格 NDC Y 方向用于 Vulkan 正高度 Viewport；
- 修正后使用 `NDC Y = pixelY / height * 2 - 1`；
- 最终 HUD 位于左上安全区，文字方向正确，未遮挡角色。

性能观察：

- 动态角色 Debug Validation、1280×720、120 个样本；
- Shadow 0.174 ms，Main Scene 0.454 ms，HUD 所在 Post-process 0.046 ms；
- 总 GPU 三阶段平均 0.674 ms；
- 与 S29 同类 Debug 数据相比，Post-process 从约 0.037 ms 增至 0.046 ms，
  本次观察增量约 0.009 ms；
- 该差值是本机单次开发验证，不作为跨设备性能保证。

视觉与确定性证据：

- 最终 HUD 图为 `captures/s31_hud_1080p_v2/frame_000002.png`；
- 1920×1080、第三帧，Timing 已完成预热；
- 大小 967,575 bytes；
- SHA-256：
  `4582AF5618802F218B94F4B38104D083A35C828071BCAF574D050963007FBAF9`；
- HUD-off 回归图与 S30 Beauty 基线 SHA-256 完全相同：
  `8FBCA478C47B094B25E6FEC3274FF6DACB6C18DF2817A77AEDCB092E9B5BA211`。

验证：

- Debug 和 Release C++ 构建通过；
- `hud.vert` 与 `hud.frag` 使用 `glslc` 编译通过；
- HUD-on 动态角色 120 帧 Debug Validation 通过；
- HUD-off 公共静态模型 120 帧 Debug Validation 通过；
- 两次测试均无 Validation warning/error；
- 最终 Manifest 已验证 `hudEnabled: true`，默认回归为 `false`；
- HUD-off 像素哈希一致，证明默认行为无视觉回归。

结论：

- S31 已提供可用于现场演示和技术录屏的 Renderer 原生 HUD；
- HUD 没有引入外部字体资产或侵入角色材质路径；
- 默认关闭策略保证此前截图和 20 秒成片继续保持干净；
- S29 性能数据和 S30 中间视图现在可以在运行中直接解释。

下一节点：

- S32 生成一段包含 Beauty、Normal、Outline、Shadow 与 HUD 的短技术拆解视频；
- 使用确定性时间步控制每个章节的起止帧；
- 保留现有 20 秒纯 Beauty 成片，技术版另行输出，不覆盖原视频。

### S32：20 秒确定性技术拆解视频

目标：把 S30 中间视图和 S31 HUD 组织成一段可直接播放的技术演示，同时
保留 S28.1 的纯 Beauty 成片，不依赖剪辑软件手工对齐。

CLI 与约束：

- 新增 `--technical-sequence`；
- 自动启用 `portfolioMode` 与 `gpuTimingEnabled`；
- 必须同时使用 `--capture-dir` 和 `--capture-frames`；
- 总帧数至少为 5，且必须能被 5 整除；
- 非法六帧测试在 Vulkan 初始化前返回退出码 1；
- 原有捕获目录和 MP4 安全不覆盖策略保持不变。

帧状态机：

- 章节索引由 `capturedFrames / (captureFrameLimit / 5)` 计算；
- 每帧在 Animation、Joint Palette 和 HUD Buffer 更新前应用章节状态；
- 章节 1：Beauty、HUD Off；
- 章节 2：World Normal、HUD Off；
- 章节 3：Internal Outline、HUD Off；
- 章节 4：Shadow Map、HUD Off；
- 章节 5：Beauty、HUD On；
- 五章统一保持 Stylized Lighting 和 Internal Outline Enabled；
- 只改变输出诊断/HUD 状态，不重置动画、相机或角色旋转。

Manifest：

- 新增 `technicalSequence`；
- 技术序列额外写出 `technicalChapters` 数组；
- 每章包含 `name`、`startFrame` 和 `endFrameExclusive`；
- 正式边界为 0、240、480、720、960、1200；
- 普通 S28/S30/S31 捕获继续输出 `technicalSequence: false`，无需章节数组。

短探针：

- Debug Validation、1280×720、60 fps、25 帧；
- 每章五帧，在 0、5、10、15、20 精确切换；
- 检查帧 0、5、10、15、20、24；
- Beauty、Normal、Outline、Shadow、Beauty + HUD 均正确；
- Manifest 经 PowerShell JSON 解析，五组起止帧与实际一致；
- 无 Validation warning/error。

正式捕获：

- Release、RTX 4060 Laptop GPU；
- 1920×1080、60 fps、1200 帧；
- 精确 20.000000 秒；
- 1200 张 `frame_%06d.png` 连续，0 缺帧；
- 原始序列 658,909,553 bytes，约 628.4 MiB；
- Manifest 正常生成并记录全部章节。

正式视觉 QA：

- 帧 120：Beauty，角色持续环绕；
- 帧 360：World Normal；
- 帧 600：Internal Outline；
- 帧 840：Shadow Map；
- 帧 960：Beauty + HUD 第一帧；
- 帧 1080 和 1199：HUD 与背面 Beauty；
- 角色在五章中保持同一连续旋转和 Idle 时间线；
- 切换没有一帧延迟；
- 没有半透明、隐藏结构穿透、蒙皮撕裂、阴影或描边分离。

编码与验证：

- 使用现有 `tools/encode_capture.ps1` 和 FFmpeg 8.1.2；
- H.264 High、CRF 15、Slow、YUV420p、Fast Start；
- BT.709 Range/Space/Transfer/Primaries；
- `ffprobe` 确认 1920×1080、60/1 fps、1200 frames、20.000000 seconds；
- MP4 大小 15,044,923 bytes；
- SHA-256：
  `184D70E40F9525AFE97825DBD11D0BA51B9FC1A8C62FD2403E6EB0199B571A75`。

结论：

- S32 已形成无需后期拼接即可复现的 Renderer 技术拆解视频；
- 五章来自同一运行实例，动画和环绕镜头在章节切换时连续；
- 原始 20 秒 Beauty 视频未修改或覆盖；
- 技术版适合放在作品集项目页、面试演示或渲染管线讲解中。

下一节点：

- S33 为技术视频增加 Renderer 原生章节标题与短暂淡入淡出；
- 标题仅在 `--technical-sequence` 中启用，不进入纯 Beauty 捕获；
- 保持章节边界和 Manifest 可复现。

### S33：Renderer 原生章节标题与确定性淡入淡出

目标：让 S32 技术拆解视频无需外部剪辑即可直接用于作品集，同时保证标题、
转场和 HUD 出现时机都由捕获帧号确定，并且不改变普通 Beauty 输出。

实现：

- 复用 S31 的 `stb_easy_font` 几何路径，不新增字体纹理或 Descriptor Set；
- 五章分别显示 `BEAUTY RENDER`、`WORLD NORMAL`、`INTERNAL OUTLINE`、
  `SHADOW MAP` 与 `BEAUTY + GPU HUD`；
- 每个标题附带一行简短技术说明；
- HUD 顶点缓冲现在同时服务技术序列的全屏过渡矩形和居中文字；
- 技术序列绘制条件不再依赖 HUD 开关，前四章也可以绘制标题与转场；
- 叠加顺序为深色过渡、章节文字、最后一章实时 HUD；
- 普通运行和普通确定性捕获仍只在显式启用 HUD 时绘制该 Pipeline。

确定性时序：

- 每章继续保持 240 帧和原有起止边界；
- 正式 60 fps 序列的淡入/淡出各为 21 帧，约 0.35 秒；
- 标题窗口为每章前 120 帧，即两秒；
- 标题自身随章节开头和标题窗口末尾渐变；
- 第五章 HUD 从局部第 21 帧开始出现，不盖住开场黑场；
- 动画、相机环绕、蒙皮 Joint Palette 和 GPU Timing 全程连续。

Manifest：

- 保留 `technicalSequence` 与 `technicalChapters`；
- 新增 `technicalFadeFrames`；
- 新增 `technicalTitleFrames`；
- 150 帧探针正确记录 10 帧过渡、30 帧标题；
- 1200 帧正式序列正确记录 21 帧过渡、120 帧标题。

探针与视觉 QA：

- Debug Validation、1280×720、60 fps、150 帧；
- 修复了一处旧绘制门槛：顶点已经生成，但前四章曾因 `hudEnabled_` 为
  `false` 而跳过 HUD Pipeline；
- 修复后检查帧 5、29、35、125、130，标题、黑场、Normal 和 HUD 时序正确；
- 正式序列检查帧 10、239、250、970、981；
- 第 239 帧完全进入深色切章画面，第 250 帧显示 World Normal 标题；
- 第 970 帧仍只有第五章标题，第 981 帧在淡入完成后显示实时 HUD；
- 未出现半透明、内部结构穿透、蒙皮撕裂或诊断视图错位。

普通 Beauty 回归：

- `captures/s33_nontechnical_regression/frame_000000.png`；
- 与 S30 基准逐像素一致；
- 两者 SHA-256 均为
  `8FBCA478C47B094B25E6FEC3274FF6DACB6C18DF2817A77AEDCB092E9B5BA211`；
- 证明 S33 只影响 `--technical-sequence`。

正式输出：

- PNG：`captures/s33_technical_titles_1080p60_20s`；
- 1920×1080、60 fps、1200 帧、0 缺帧；
- 原始序列 635,212,913 bytes；
- MP4：
  `captures/Afterglow_S33_TechnicalTitles_1080p60_20s.mp4`；
- MP4 大小 16,007,831 bytes；
- SHA-256：
  `9915F887B0B70350711E6243AD357DB076FEB302A76CBA8D53D43AC4207B0DCB`；
- H.264 High、YUV420p、BT.709、60/1 fps、1200 frames、20.000000 seconds。

验证：

- Debug 与 Release 构建通过；
- 150 帧私有动态角色 Debug Validation 探针通过；
- 120 帧公开静态资产 Debug Validation 回归通过；
- 正式 PNG 连续性、Manifest JSON 与 MP4 媒体规格通过；
- S32 技术视频 SHA-256 仍为
  `184D70E40F9525AFE97825DBD11D0BA51B9FC1A8C62FD2403E6EB0199B571A75`，
  未被覆盖。

结论：

- S33 已把原生技术序列提升为可直接交付的作品集视频；
- 标题和转场属于 Renderer 输出，而不是后期合成；
- 可复现帧边界和 Manifest 仍然保留；
- 纯 Beauty 路径保持零像素回归。

下一节点：

- S34 为作品集交付建立统一索引，整理 Beauty、Technical、截图和性能报告；
- 为视频生成代表性封面/联系表，减少面试展示时的查找成本；
- 不改变当前渲染画面，先把已完成成果整理成可投递结构。

### S34：作品集交付索引、封面与技术联系表

目标：把现有 Renderer 成果整理为面试和投递时可快速使用的入口，不修改
渲染算法，也不复制已有的大型视频和 PNG 序列。

交付结构：

- 新增 `portfolio/README_CN.md`；
- 新增 `portfolio/portfolio_manifest.json`；
- 新增 `portfolio/images/afterglow_cover_1920x1080.png`；
- 新增 `portfolio/images/technical_contact_sheet_1920x1080.png`；
- 新增 `tools/build_portfolio_package.ps1`，可从正式输出重新生成全部派生内容。

作品集入口：

- README 给出封面、Beauty 视频、Technical 视频、联系表和性能报告的推荐展示顺序；
- 提供一段可直接用于项目页的一句话说明和技术亮点；
- 明确 S29 GPU Timestamp 仅覆盖三个 GPU Pass，不把 0.881180 ms 错写成
  完整端到端帧耗时；
- 记录角色资产只用于非商业技术展示，公开源码不重新分发私有 GLB/贴图。

封面：

- 来源为 S33 正式序列第 120 帧的干净 Beauty 输出；
- 1920×1080 RGBA PNG；
- 左侧深色信息区保留项目名、技术方向、作者和 FYP 年份；
- 右侧保留角色和 Endfield Industrial 展示背景；
- 文件大小 642,759 bytes；
- SHA-256：
  `BF076B2EB90642C34B4B5E55C033DB328C1ED0F957853898373CE2E709712EB0`。

技术联系表：

- 取 S33 正式序列第 30、270、510、750、990 帧；
- 以 3×2 网格展示 Beauty、World Normal、Internal Outline、Shadow Map 和
  Beauty + GPU HUD；
- 第六格概括 Shadow/Main/Outline Pipeline；
- 顶部标注确定性 1080p60 五视图，底部列出核心技术；
- 1920×1080 RGBA PNG；
- 文件大小 474,849 bytes；
- SHA-256：
  `84C542602126FA9E27188698287E5622E86C594566C9421F8223908ACF4EFBF7`。

机器可读清单：

- 格式为 `Afterglow portfolio package v1`，里程碑为 S34；
- 收录 Beauty MP4、Technical MP4、封面、联系表和 Release Timing JSON；
- 每个 Artifact 记录用途、项目相对路径、字节数和 SHA-256；
- 技术视频记录 1920×1080、60 fps、1200 帧、20 秒、五章边界、21 帧
  Fade 和 120 帧 Title；
- 性能摘要记录 RTX 4060 Laptop GPU、600 样本以及各 Pass 平均耗时；
- 所有五个路径均已解析验证，不存在缺失项。

工具兼容性：

- 初版使用 `.NET Path.GetRelativePath`，Windows PowerShell 5.1 不支持；
- 已改为先验证 Artifact 位于项目根目录下，再安全截取相对路径；
- FFmpeg 负责封面文字排版和六格联系表；
- 脚本会先检查全部输入，缺失时拒绝生成不完整清单；
- 重跑成功，三项输出均刷新。

验证：

- 两张图片使用 `ffprobe` 确认均为 1920×1080 RGBA PNG；
- 两张图经过人工视觉 QA，文字无裁切、角色构图清晰、五种视图可辨认；
- Manifest 使用 PowerShell JSON 解析通过；
- 五个 Artifact 路径全部存在；
- Manifest 中技术视频为 1200 帧，GPU Timing 为 600 样本；
- `build_portfolio_package.ps1` 在 Windows PowerShell 5.1 下完整执行成功。

结论：

- S34 已提供一个无需搜索 `captures/` 的作品集统一入口；
- 面试时可以按 Cover → Beauty → Technical → Contact Sheet → Timing 顺序展示；
- 大型 MP4 没有重复复制，交付索引保持轻量；
- Renderer、角色材质和 S33 正式视频没有被修改。

下一节点：

- S35 优先执行 AzureRender 命名迁移与逻辑不变重构；
- Tone Mapping 顺延到 S36；
- 先完成可验证的构建目标、主类与外部品牌迁移，再拆分大型实现文件。

### S35.1：AzureRender 命名迁移第一阶段

目标：将占位工程名 `MyVulkanApp` 和通用主类名 `VulkanApp` 迁移为
`AzureRender` 品牌，同时保持渲染算法、资源所有权、初始化/销毁顺序、
Shader 和持久化数据协议不变。

本阶段完成：

- CMake Project 与可执行 Target：`MyVulkanApp` → `AzureRender`；
- Shader Target：`MyVulkanAppShaders` → `AzureRenderShaders`；
- Debug/Release 输出：`AzureRender.exe`；
- 编译定义：`MYVULKANAPP_*` → `AZURERENDER_*`；
- `VulkanRunOptions` → `AzureRenderOptions`；
- `VulkanApp.hpp/.cpp` → `AzureRenderApp.hpp/.cpp`；
- `VulkanApp` → `AzureRenderApp`；
- GLFW 窗口标题、Vulkan Application/Engine Name 与 HUD 标题改为 AzureRender；
- F1 展示预设的显示名改为 `Azure Gallery`；
- 公共测试 glTF 的 generator 元数据改为 AzureRender；
- CLI Usage 和所有当前 README 运行命令改用 `AzureRender.exe`。

明确保留的 Legacy Schema：

- `Afterglow PNG sequence v1`；
- `Afterglow GPU timing v1`；
- glTF `afterglowAoColor`、`afterglowMatcapTexture`、
  `afterglowHairDataTexture` 等 extras；
- 私有 GLB 中的 `Afterglow_ProceduralIdle`；
- S28、S32、S33 历史视频文件名及其 SHA-256。

保留这些名称可以让现有 GLB、捕获目录、编码脚本和作品集证据继续工作。
后续如升级为 AzureRender v2 数据格式，必须增加向后兼容读取，而不是直接
替换旧键。

构建验证：

- 重新执行 `cmake --preset ninja-debug` 与 `ninja-release`；
- Debug 和 Release 均成功编译、链接 `AzureRender.exe`；
- 新 Target 使用 `AzureRenderApp.cpp`、`AzureRenderApp.hpp`；
- 公共测试模型 120 帧 Debug Validation 通过，无 warning/error。

像素与兼容性回归：

- 新输出：`captures/s35_azure_rename_regression/frame_000000.png`；
- 与 S30 Stylized Beauty 基准逐像素一致；
- 两者 SHA-256 均为
  `8FBCA478C47B094B25E6FEC3274FF6DACB6C18DF2817A77AEDCB092E9B5BA211`；
- 新 Manifest 仍为 `Afterglow PNG sequence v1`；
- 动画仍解析为 `Afterglow_ProceduralIdle`；
- S28 Beauty MP4 哈希仍为
  `5305A559FDC425FB2E2A3CD65C947291A6B251CC1CDB68FA015B28268BC40F29`；
- S33 Technical MP4 哈希仍为
  `9915F887B0B70350711E6243AD357DB076FEB302A76CBA8D53D43AC4207B0DCB`。

技术序列回归：

- 私有动态角色、1280×720、25 帧 Debug Validation；
- 五章在 0、5、10、15、20 帧切换；
- GPU Timestamp 25 个样本正常完成；
- 第 24 帧 HUD 显示 `AZURERENDER VULKAN RENDERER`；
- 极短探针最后一帧处于章节淡出黑场，符合 S33 帧时序；
- 未发现半透明、隐藏结构穿透、蒙皮撕裂或章节状态错误。

作品集品牌同步：

- 新封面：`portfolio/images/azurerender_cover_1920x1080.png`；
- 封面 SHA-256：
  `90D1041D124C6C9A3D7F5860FEEEE70FFAC8D3089DE7D44AC46D0D05442F85EF`；
- 技术联系表 SHA-256：
  `66BB472CB6F301D7CA7C2EABD5D05E885D2E799F2AA7DCCBA31E5A955F628DA7`；
- `portfolio_manifest.json` 格式改为 `AzureRender portfolio package v1`；
- 五个 Artifact 路径与哈希全部重新验证通过；
- S34 的旧 Afterglow 封面仍保留，不删除历史证据。

结论：

- 第一阶段命名迁移实现了零 Beauty 像素回归；
- 代码执行顺序和 Vulkan 资源生命周期未改变；
- 新品牌与旧数据协议已经明确分层；
- 工程根目录仍暂时保留 `Project/MyVulkanApp`，避免硬编码工具路径同时失效。

下一节点：

- S35.2 将 `AzureRenderApp.cpp` 按职责拆成多个 Translation Unit；
- 继续保留同一个类和全部成员，不迁移资源所有权；
- 每次拆分后重复 Debug/Release、公共 Validation 和 Beauty 哈希回归；
- 全部稳定后再消除工具绝对路径并把工程目录改为 `Project/AzureRender`。

### S35.2：第一轮 Translation Unit 拆分

目标：降低 `AzureRenderApp.cpp` 的单文件规模，但保持 `AzureRenderApp` 为唯一
资源所有者，不移动成员字段、不引入新 Subsystem，也不改变任何成员函数体、
调用顺序或 Vulkan Handle 销毁顺序。

依赖分析：

- 原文件约 4,530 行；
- 顶部匿名命名空间包含矩阵数学、展示地台、Validation/Extension 常量、
  `vkCheck` 和 Debug Messenger 辅助函数；
- Frame/Command Recording 强依赖矩阵辅助，首轮不移动；
- Support 与 Capture 仅需要少量标准库头和本地 `vkCheck`，适合作为低风险边界。

新增 Translation Unit：

- `AzureRenderSupport.cpp`，610 行；
- `AzureRenderCapture.cpp`，340 行；
- `AzureRenderApp.cpp` 降至 3,624 行；
- 三个 `.cpp` 仍共同实现同一个 `AzureRenderApp` 类；
- CMake Target 同时编译三个文件，没有新静态库或运行时组件。

Support 职责：

- Swapchain recreate/cleanup；
- Queue Family、Surface Format、Present Mode、Extent 和 Memory Type 查询；
- Validation Layer 与 Device Extension 检查；
- Buffer Copy、Image Layout Transition、Buffer-to-Image Copy；
- Image/Image View、Shader Module 与 Binary File 支持；
- Framebuffer Resize 和 Vulkan Debug Callback。

Capture 职责：

- 捕获目录安全检查；
- PNG Capture Manifest；
- GPU Timestamp 收集、控制台摘要与 JSON；
- Swapchain Image Readback 和 PNG Screenshot。

编译中发现并修复：

- 第二批 Support 方法最初被补丁插入到匿名命名空间结束符之前；
- MinGW 正确报告成员定义不在 `AzureRenderApp` 所属命名空间；
- 只移动 `}  // namespace` 到 `vkCheck` 之后，未修改任何函数体；
- 随后 Debug/Release 均成功编译和链接。

回归：

- 公共测试模型 120 帧 Debug Validation 通过；
- 私有角色 Release Beauty：
  `captures/s35_split_beauty_regression/frame_000000.png`；
- 与 S30 基准 SHA-256 均为
  `8FBCA478C47B094B25E6FEC3274FF6DACB6C18DF2817A77AEDCB092E9B5BA211`；
- 因而文件拆分保持零 Beauty 像素回归；
- 私有角色 25 帧 Debug Technical Sequence 通过；
- 五章、1 帧 Fade、5 帧 Title 和 25 个 GPU Timing Sample 均正常；
- Capture Manifest 继续使用 `Afterglow PNG sequence v1` Legacy Schema；
- 无 Validation warning/error、半透明、隐藏结构穿透或蒙皮错误。

结论：

- 第一轮拆分已经验证多个 Translation Unit 可以安全共同实现现有类；
- 没有新增资源所有权或生命周期；
- Support/Capture 的修改范围以后可以独立定位；
- 复杂的 Frame/Pipeline 区域留待下一轮小步拆分。

下一节点：

- S35.3 拆出 `AzureRenderFrame.cpp`，包含 Draw、UBO/HUD 更新和 Command Recording；
- 为矩阵数学与 `vkCheck` 建立只含 Inline Helper 的内部头；
- 仍不改变函数体和资源成员；
- 验证后再拆 Pipeline/Resource Creation，最后迁移工程根目录。

### S35.3：Frame Translation Unit 与内部 Inline Helper

目标：继续降低 `AzureRenderApp.cpp` 的单文件复杂度，把每帧执行路径放到明确的
Translation Unit，同时严格保持现有类、成员字段、函数体、调用顺序和 Vulkan
资源生命周期不变。

结构变化：

- 新增 `AzureRenderInternal.hpp`，集中 `Matrix4`、`Vector3`、矩阵/向量运算与
  `vkCheck`；
- Helper 全部为 `inline`，内部头不持有状态、不创建资源，也不形成新 Subsystem；
- `AzureRenderApp.cpp`、`AzureRenderCapture.cpp` 和 `AzureRenderSupport.cpp` 删除
  重复的本地 `vkCheck` 实现，统一使用内部 Helper；
- 新增 `AzureRenderFrame.cpp`，包含 `drawFrame`、`updateUniformBuffer`、
  `updateHudBuffer` 与 `recordCommandBuffer`；
- CMake Target 增加新 Translation Unit 与内部头；
- `AzureRenderApp.cpp` 从 S35.2 的 3,624 行降至 2,545 行；
- `AzureRenderFrame.cpp` 为 979 行，四个 `.cpp` 继续共同实现同一个
  `AzureRenderApp` 类。

编译检查：

- 第一次编译准确发现 Frame 文件缺少 `<filesystem>`；
- 只补充直接依赖头，没有调整函数体；
- Ninja Debug 与 Release 随后均成功编译和链接 `AzureRender.exe`；
- 全工程只保留一处 `vkCheck`、一处 `Matrix4` 和一处 `Vector3` 定义，均位于
  `AzureRenderInternal.hpp`。

回归：

- 公共 `assets_public/test_model.gltf` 完成 120 帧 Debug Validation，退出码 0；
- 私有角色 Release Beauty 输出：
  `captures/s35_frame_beauty_regression/frame_000000.png`；
- 输出 SHA-256 与 S30 基准完全一致：
  `8FBCA478C47B094B25E6FEC3274FF6DACB6C18DF2817A77AEDCB092E9B5BA211`；
- 私有角色 25 帧 Debug Technical Sequence 位于
  `captures/s35_frame_technical_probe`；
- 五章边界为 0、5、10、15、20 帧，Manifest 完整记录 25 帧；
- GPU Timestamp 收集 25 个样本，Shadow/Main/Outline 平均约为
  0.178/0.504/0.040 ms，本次探针总 GPU Render 平均约 0.722 ms；
- 运行时仍报告 81,487 vertices、284,673 indices、14 primitives、15 materials、
  468 Joint Matrix 和 `Afterglow_ProceduralIdle`；
- 未出现 Validation warning/error、Beauty 像素变化、半透明、隐藏结构穿透、
  材质丢失或蒙皮撕裂。

结论：

- Frame 执行路径已经拥有独立、可定位的代码边界；
- 重构没有创建第二套资源所有权，也没有改变 Command Buffer 的录制顺序；
- 内部 Helper 消除了跨 Translation Unit 的重复实现；
- 当前重构仍是零逻辑、零 Beauty 像素变化。

下一节点：

- S35.4 优先拆分 Pipeline/Render-Pass/Framebuffer Creation；
- 保留同一个 `AzureRenderApp` 与所有 Handle 字段，不抽象虚基类或新所有者；
- 重复 Debug/Release、公共 Validation、Technical Probe 与 Beauty 哈希回归；
- Pipeline 边界稳定后再拆 Descriptor/Buffer/Image Resource Creation；
- 最后清理工具中的工程绝对路径，再迁移根目录为 `Project/AzureRender`。

### S35.4：Pipeline Creation Translation Unit

目标：把 Render Pass、Graphics Pipeline 和 Swapchain Framebuffer 创建集中到
独立实现文件，让 Pipeline 状态拥有清晰的代码边界，同时保持所有 Vulkan Handle
仍由 `AzureRenderApp` 持有。

移动范围：

- `createRenderPass`；
- `createPostProcessRenderPass`；
- `createGraphicsPipeline`；
- `createFramebuffers`；
- `createPostProcessFramebuffers`。

明确未移动：

- `createShadowResources`，因为它同时创建 Shadow Image、Memory、View、Sampler、
  Render Pass 与 Framebuffer，留待资源创建边界统一处理；
- Descriptor Set Layout/Pool/Set；
- Depth/Normal/Texture/Buffer 等 GPU Resource Creation；
- Pipeline/Render Pass/Framebuffer 的成员字段和 Cleanup 销毁代码。

结构结果：

- 新增 `AzureRenderPipeline.cpp`，658 行；
- `AzureRenderApp.cpp` 从 2,545 行降至 1,900 行；
- CMake Target 增加 Pipeline Translation Unit；
- Pipeline Shader 读取、Shader Module 异常清理、Pipeline Variant 创建和
  Framebuffer attachment 顺序均保持原函数体不变；
- 所有 `AzureRenderApp` 成员函数定义经过全工程审计，没有重复定义。

构建与运行回归：

- Ninja Debug 与 Release 均成功编译、链接；
- 公共 `assets_public/test_model.gltf` 完成 120 帧 Debug Validation，退出码 0；
- 私有角色 Release Beauty 输出：
  `captures/s35_pipeline_beauty_regression/frame_000000.png`；
- SHA-256 与 S30/S35.1/S35.2/S35.3 基准完全一致：
  `8FBCA478C47B094B25E6FEC3274FF6DACB6C18DF2817A77AEDCB092E9B5BA211`；
- 私有角色 Debug Technical Sequence 位于
  `captures/s35_pipeline_technical_probe`，25 帧与五章 Manifest 完整；
- 收集 25 个 Timestamp 样本，证明 Shadow/Main/Outline Query 路径仍可用；
- 本次短 Debug 探针测得 Shadow/Main/Outline 平均约为
  2.854/7.437/0.190 ms，总计平均 10.481 ms，范围 0.633–20.798 ms；
- 该探针启用 Validation、样本仅 25 帧且没有正式预热，波动不用于判断性能回归；
- 正式性能比较仍必须使用固定设备、Release、预热和长序列协议；
- 无 Validation warning/error、Beauty 像素变化、半透明、隐藏结构穿透、
  材质丢失或蒙皮撕裂。

结论：

- Pipeline 创建代码已获得独立修改边界；
- 本轮没有更改 Attachment、Subpass Dependency、Blend/Depth/Raster State 或
  Shader 组合；
- Pipeline Handle 所有权和 Cleanup 仍集中在原类；
- 文件移动继续保持零 Beauty 像素变化。

下一节点：

- S35.5 先拆 Descriptor Set Layout/Pool/Set；
- 再拆 Vertex/Index/Uniform/Joint/HUD Buffer 与 Texture Resource Creation；
- Shadow Resources 根据依赖放入 Resource Translation Unit，不拆散其创建顺序；
- 完成后重复 Debug/Release、公共 Validation、Technical Probe 与 Beauty 哈希；
- 资源边界稳定后再处理绝对路径和工程根目录改名。

### S35.5：Descriptor 与 GPU Resource Creation 拆分

目标：完成 `AzureRenderApp.cpp` 中数据资源创建代码的职责拆分，同时保持
Descriptor Binding、资源数量、Image Layout、上传顺序和 Handle 所有权不变。

第一批——Descriptor：

- 新增 `AzureRenderDescriptors.cpp`；
- 移入 `createDescriptorSetLayout` 与
  `createPostProcessDescriptorSetLayout`；
- 移入 `createDescriptorPool`、`createDescriptorSets` 与
  `createPostProcessDescriptorSets`；
- Binding 0–10、每材质/每帧 Set 索引和 Post-process Binding 0–2 均未改变；
- 第一批完成后单独执行 Debug 编译，成功链接。

第二批——GPU Resource：

- 新增 `AzureRenderResources.cpp`；
- 移入 Swapchain Image View、Depth、Normal 和 Shadow Resource 创建；
- 移入 Vertex/Index Buffer、全部 Material/Environment Texture 上传；
- 移入 Uniform、Joint Storage 与 HUD Vertex Buffer 创建；
- Shadow Image、Sampler、Render Pass 与 Framebuffer 保持在同一函数内，未拆散
  创建顺序；
- Command Pool/Buffer、同步对象和 Timestamp Query 留在主实现文件。

结构结果：

- `AzureRenderDescriptors.cpp`：377 行；
- `AzureRenderResources.cpp`：497 行；
- `AzureRenderApp.cpp`：由 1,900 行降至 1,054 行；
- CMake Target 同时编译七个 `AzureRender*.cpp` 实现文件；
- 所有文件继续共同实现同一个 `AzureRenderApp`，没有新增资源所有者；
- 全工程成员函数定义审计无重复。

回归：

- Ninja Debug/Release 编译和链接成功；
- 公共测试模型 120 帧 Debug Validation 通过；
- 私有角色 Release Beauty：
  `captures/s35_resource_beauty_regression/frame_000000.png`；
- SHA-256 与全部既有 S30/S35 基准一致：
  `8FBCA478C47B094B25E6FEC3274FF6DACB6C18DF2817A77AEDCB092E9B5BA211`；
- 私有角色 25 帧 Debug Technical Sequence 位于
  `captures/s35_resource_technical_probe`；
- Manifest 声明 25 帧，目录实际包含 25 张 PNG，五章完整；
- Timestamp 25 个样本，本次 Shadow/Main/Outline 平均约为
  1.417/3.544/0.104 ms，总计平均 5.065 ms，范围 0.627–18.570 ms；
- 短 Debug Validation 探针未预热且波动明显，不用于性能结论；
- 运行时仍为 81,487 vertices、284,673 indices、14 primitives、15 materials、
  468 Joint Matrix 与 `Afterglow_ProceduralIdle`；
- 无 Validation warning/error、Beauty 像素变化、Descriptor 错配、半透明、
  隐藏结构穿透、材质丢失或蒙皮撕裂。

结论：

- Descriptor 与 GPU 数据资源创建已有清晰、独立的实现边界；
- 主实现文件已缩小到可维护规模；
- 本轮没有修改资源布局、Binding ABI 或 Cleanup 生命周期；
- S35.1–S35.5 的品牌迁移与代码拆分保持零 Beauty 像素变化。

下一节点：

- S35.6 先完成工程目录迁移前的绝对路径与 `MyVulkanApp` 引用审计；
- 将活动脚本改为基于脚本位置/项目根目录解析路径；
- 区分需要迁移的活动引用与必须保留的历史 Artifact 名称；
- 再跑完整构建与回归，确认路径可移植；
- 最后受控地把工程目录改名为 `Project/AzureRender`。

### S35.6：工程目录迁移前路径审计

目标：在移动根目录前消除所有活动代码/工具对 `MyVulkanApp` 目录名的依赖，并用
另一目录名的全新构建证明项目可移植，而不是依赖现有 CMake Cache 得出结论。

审计分类：

- 活动路径：CMake、Presets、源码、Shader、工具脚本和作品集构建入口；
- 当前操作说明：交接文档顶部与本机 `cd` 命令，实际改名后更新；
- 历史证据：S35.1 的 `MyVulkanApp → AzureRender` 迁移记录，继续保留；
- 排除项：`build/` Cache、`captures/`、`assets_private/` 与 Git 元数据。

结果：

- CMake 与 Presets 已基于 source directory 解析，无旧目录名依赖；
- PowerShell 编码/作品集工具无旧目录名依赖；
- 七个 Unreal Python 工具发现硬编码旧绝对路径；
- 七个工具均新增 `resolve_project_root()`：优先读取
  `AZURERENDER_PROJECT_ROOT`，否则从 `__file__` 所在 `tools/` 目录向上解析；
- 若两种来源都不可用，工具明确报错并要求设置环境变量，不再静默写入旧目录；
- 资产输出文档改用项目相对路径。

工具验证：

- 使用工作区 Python 对七个脚本执行 AST parse，全部通过；
- 隔离执行每个 `resolve_project_root()`，脚本相对解析全部通过；
- 环境变量 override 测试通过；
- 活动源码与工具中旧绝对工程路径命中为 0；
- 剩余旧目录名只存在于历史日志和尚未执行改名的当前交接路径。

改名探针：

- 创建临时 `Project/AzureRender_s35_path_probe`；
- 仅复制 CMake/Presets/vcpkg、`src/`、`shaders/` 与 `assets_public/`；
- 初次 Preset 配置发现当前进程缺少 `VCPKG_ROOT` 且 Ninja 不在 PATH；
- 设置真实 vcpkg/Ninja 路径后，沙箱阻止 vcpkg Manifest 获取全局 lock；
- 不修改全局 vcpkg 状态，改用已安装 `x64-windows` prefix 进行只读配置；
- 全新 Debug 与 Release 各完成 12 个 Shader + 9 个 C++ Object + 1 个 Link，
  共 22 步；
- Debug 公共资产 120 帧 Validation 通过；
- Release 从探针目录加载原私有 GLB，81,487 vertices、284,673 indices、
  14 primitives、15 materials、468 Joint Matrix 均正常；
- 输出 `captures/s35_path_probe_beauty_regression/frame_000000.png`；
- SHA-256 与基准完全一致：
  `8FBCA478C47B094B25E6FEC3274FF6DACB6C18DF2817A77AEDCB092E9B5BA211`；
- 验证后删除临时探针目录，避免产生第二份工程真相来源。

结论：

- 活动源码、构建定义和工具已经不依赖 `MyVulkanApp` 目录名；
- 改名根目录中的全新构建与运行已经得到实际证明；
- 旧 build Cache 包含旧绝对路径，正式改名后必须重新配置；
- 当前已满足执行受控根目录改名的前置条件。

下一节点：

- S35.7 检查 `Project/AzureRender` 目标不存在；
- 同卷移动 `Project/MyVulkanApp` 到 `Project/AzureRender`；
- 从新路径生成新的 Debug/Release Cache；
- 运行公共 Validation、私有 Beauty 与 Technical Sequence；
- 更新所有当前操作路径，同时保留历史迁移证据。

### S35.7：AzureRender 根目录正式迁移

目标：完成 S35 最后一项外部身份迁移，把工程根目录从占位名改为正式品牌名，并在
全新 CMake Cache 下证明构建、资产加载与渲染输出没有路径回归。

受控移动：

- 源：`D:\Assigment\2609\FYP\Project\MyVulkanApp`；
- 目标：`D:\Assigment\2609\FYP\Project\AzureRender`；
- 移动前确认源存在、目标不存在，且两者均位于预期 `Project/` 根目录下；
- 从父目录执行同卷移动，完成后旧目录不存在；
- 没有复制或重新生成私有资产，Git 工作树内容整体随目录移动。

Cache 迁移：

- 原 `ninja-debug` 与 `ninja-release` 暂存为 `*-pre-rename`；
- 设置 `VCPKG_ROOT=C:\Users\23587\.vcpkg-clion\vcpkg`；
- PATH 增加 CLion Ninja 与 MinGW；
- 标准 `cmake --preset ninja-debug/ninja-release` 成功运行 vcpkg Manifest；
- Debug/Release 均完成 12 个 Shader、9 个 C++ Object 和 1 个 Link，共 22 步；
- 两个新 Cache 的 Source/Home Directory 均为 `Project/AzureRender`；
- 全部回归通过后删除 `*-pre-rename`，只移除可重建构建产物。

运行回归：

- 公共资产 120 帧 Debug Validation 通过；
- 私有 Release Beauty 输出：
  `captures/s35_root_rename_beauty_regression/frame_000000.png`；
- SHA-256 与 S30 及 S35.1–S35.6 基准完全一致：
  `8FBCA478C47B094B25E6FEC3274FF6DACB6C18DF2817A77AEDCB092E9B5BA211`；
- 私有 25 帧 Debug Technical Sequence 位于
  `captures/s35_root_rename_technical_probe`；
- 五章、25 张 PNG、25 个 Timestamp Sample 均正常；
- 本次短 Debug Shadow/Main/Outline 平均约为 2.408/5.813/0.160 ms，
  总计 8.380 ms，范围 0.628–19.744 ms，仅作功能探针；
- 角色统计、Joint Palette、Animation Legacy Name 和 Capture Schema 均未改变；
- 无 Validation warning/error、路径失效、Beauty 像素变化、半透明、隐藏结构穿透、
  材质丢失或蒙皮撕裂。

文档同步：

- 交接摘要的主工程路径与 `cd` 命令改为 `Project/AzureRender`；
- README 增加 S35.7 状态；
- Unreal 工具继续使用脚本相对根目录，无需再次修改；
- S35.1/S35.6 中的旧目录名保留为历史迁移证据。

结论：

- AzureRender 品牌迁移已覆盖构建目标、类型、运行时 UI、代码结构和项目根目录；
- S35 全部阶段保持零 Beauty 像素变化；
- 活动构建与工具不再依赖 `MyVulkanApp`；
- S35 重构阶段正式结束。

下一节点：

- S36.1 记录当前 LDR Color Pipeline 基线；
- 选择 HDR Scene Color 格式、曝光模型与 Tone Mapping 算法；
- 先定义新的视觉/数值验收方法，再修改 Attachment 和 Shader；
- 保留当前 S35 Beauty 哈希作为“重构不变”最终基线；
- Tone Mapping 引入后的预期像素变化使用新基准，不再要求匹配 S30。

### S36.1：LDR 色彩管线基线与 HDR 设计冻结

目标：在首次有意改变 Beauty 前，记录当前色彩路径、量化最终 LDR 输出、确认目标
格式能力，并冻结可直接实施的 HDR/Tone Mapping 方案。

当前路径事实：

- Swapchain 优先 `VK_FORMAT_B8G8R8A8_SRGB` +
  `VK_COLOR_SPACE_SRGB_NONLINEAR_KHR`；
- Main Pass 直接写 Swapchain Color，同时写 Depth 与 RGBA8 World Normal；
- Post-process 对同一个 Swapchain Image 使用 `LOAD`；
- Internal Outline 以 Alpha Blend 叠加；
- HUD/Title/Fade 随后写入同一个 Post-process Pass；
- 没有可采样 HDR Scene Color，Main Shader 高于 1.0 的输出会在 SRGB Attachment
  存储时截断。

冻结基线：

- `captures/s36_ldr_color_baseline/frame_000000.png`；
- 1920×1080 RGBA；
- SHA-256：
  `8FBCA478C47B094B25E6FEC3274FF6DACB6C18DF2817A77AEDCB092E9B5BA211`；
- RGB min 16/24/26，max 255/210/221，mean 39.073/63.402/69.578；
- Linear Luminance P50/P90/P95/P99/P99.9 为
  0.043297/0.060778/0.096704/0.264917/0.316922；
- 任一通道 255 占 0.000048%，Alpha 非 255 为 0%。

格式与代码探针：

- 选择 `VK_FORMAT_R16G16B16A16_SFLOAT`；
- `AzureRenderApp` 增加目标格式常量与支持状态；
- Physical Device 选择后查询 optimal tiling Format Features；
- 必需 Sampled Image、Color Attachment、Color Attachment Blend；
- 当前 RTX 4060 Laptop GPU 报告 `supported`；
- `vulkaninfoSDK --show-formats` 同时确认上述能力；
- 本节点只探测，不启用 HDR Attachment，因此 Beauty 像素保持不变。

设计决策：

- Main Scene 写 per-Swapchain RGBA16F；
- Post-process 新增 Binding 3 采样 Scene Color；
- Beauty 在线性 HDR 中合成 Internal Outline；
- 0 EV Exposure 后使用 Narkowicz ACES fitted；
- 输出线性 0–1，由 SRGB Swapchain 自动编码；
- World Normal/Internal Outline/Shadow Map 诊断绕过 Tone Mapping；
- HUD/Title/Fade 位于 Tone Mapping 后；
- 1080p 每张 Scene Color 约 15.82 MiB，三张约 47.46 MiB；
- 不做静默格式 fallback，移动端策略延后到能力层。

验证：

- Ninja Debug/Release 编译成功；
- 公共资产 120 帧 Debug Validation 通过；
- 启动打印
  `HDR scene color candidate: VK_FORMAT_R16G16B16A16_SFLOAT (supported)`；
- 私有 Release LDR 基线与 S35 SHA-256 完全一致；
- 未改变 Attachment、Descriptor、Shader 输出或 Capture Schema。

文档：

- 新增 `docs/HDR_TONEMAPPING_DESIGN_CN.md`；
- 包含资源生命周期、Render Pass、Descriptor、Shader、诊断/HUD 规则、显存估算、
  功能/视觉/性能验收和分步实施方案。

下一节点：

- S36.2 实现 Scene Color Resource 与 Swapchain Lifecycle；
- Main Framebuffer attachment 0 改为 RGBA16F；
- Post-process Binding 3 采样 Scene Color；
- Final Composite 实现 Internal Outline + 0 EV ACES fitted；
- 完整检查 Resize、诊断视图、HUD、Capture 与 Validation；
- 建立预期变化后的 `S36 HDR Beauty v1` 新基准。

### S36.2：HDR Scene Color 与固定 Tone Mapping

目标：把 S36.1 的冻结设计变为活动渲染路径，并建立首次有意改变 Beauty 后的新基准。

资源与生命周期：

- 新增 per-Swapchain `sceneColorImages_`、`sceneColorImageMemories_` 与
  `sceneColorImageViews_`；
- 使用 `VK_FORMAT_R16G16B16A16_SFLOAT`、Color Attachment + Sampled Usage；
- 初始创建与 Swapchain Recreate 均在 Image View 后、Depth/Normal 前创建 Scene Color；
- Cleanup 在 Framebuffer 销毁后释放全部 Scene Color View/Image/Memory；
- 目标 GPU 格式能力不满足时启动明确失败，不使用不可见的 LDR fallback；
- 完成资源阶段后单独执行 Ninja Debug 构建，成功。

Render Pass 与 Descriptor：

- Main Color Attachment 从 Swapchain Format 改为 RGBA16F，结束布局为 Shader Read；
- Main Framebuffer attachment 0 改为 Scene Color View；
- Final Pass 对 Swapchain 使用 `DONT_CARE` load 与 `UNDEFINED` initial layout；
- Post-process Layout 增加 Binding 3 Scene Color；
- Descriptor Pool 采样器容量由每 Set 3 增至 4；
- 每张 Swapchain Image 的 Post-process Set 绑定同索引 Scene Color View。

Final Composite：

- `PostProcessPushConstants` 从 16 bytes 增至 32 bytes；
- 新增 0 EV Exposure、Tone Mapping Enabled 与两个显式 Padding；
- Beauty 先在线性 HDR 中混合 Internal Outline，再执行 Narkowicz ACES fitted；
- SRGB Swapchain 负责最终编码，Shader 不手动 Gamma；
- Final Composite Pipeline 关闭 Alpha Blend，并固定 Alpha 为 1；
- HUD Pipeline 恢复并保持 Alpha Blend；
- World Normal、Internal Outline、Shadow Map 绕过 Exposure/ACES；
- HUD、章节标题与 Fade 继续在 Final Composite 后绘制。

构建与运行回归：

- Ninja Debug 与 Release 构建成功；
- 公共 `assets_public/test_model.gltf` 完成 120 帧 Debug Validation，退出码 0；
- 使用实际 Windows 窗口完成 856×511 到 1707×912 的最大化 Resize，并执行最小化与
  恢复；每次状态变化后画面继续更新，证明 Swapchain Recreate 与零尺寸等待路径可用；
- GPU 报告 RGBA16F 候选 `supported`；
- 私有 Release Beauty 位于
  `captures/s36_hdr_beauty_v1/frame_000000.png`；
- 新 SHA-256 为
  `5E8BF8B507FE07F385EAADF563DF40CD3C23FA6A2433156DEFD1BFD6AB829357`；
- RGB min 5/11/13、max 235/217/222、mean 29.000/60.084/69.169；
- Linear Luminance P50/P90/P95/P99/P99.9 为
  0.037679/0.062127/0.120969/0.394080/0.457723；
- 任一 RGB 通道为 255 的像素比例 0%，Alpha 非 255 为 0%；
- 与 S36.1 LDR 基准相比，低值背景更深、高光受到平滑压缩，属于预期 Tone Mapping
  变化；角色遮挡、材质、阴影、蒙皮和轮廓保持正确，没有半透明或隐藏结构穿透；
- 私有 Debug `captures/s36_hdr_technical_probe` 完成 25 帧、五章节、25 个 Timestamp；
- 代表帧确认三个诊断视图不受 ACES 污染，Beauty+HUD 的叠加顺序正确；
- 短 Debug Shadow/Main/Final 平均 1.301/3.262/0.142 ms，总计 4.706 ms；
- 无 Validation warning/error、Descriptor/Layout 错误或 Capture 回归。

结论：

- AzureRender 已从“直接写 LDR Swapchain”切换为可扩展的线性 HDR Scene Color；
- S35/S36.1 哈希仍保留为重构与 LDR 历史基准，S36.2 起使用新 HDR Beauty 基准；
- 当前链路已具备后续 Exposure、Bloom、HDR IBL 与 Tone Mapper 对比的必要中间纹理；
- S36.2 完成。

下一节点：

- 该处原定 S36.3 已被 2026-08-02 的长期路线重排覆盖；详见下方 M0 记录。

## M0 — 长期开发主计划 v2.0 与质量治理冻结

日期：2026-08-02

背景：

- 连续按局部“下一步”推进容易把技术实现数量误当作项目完成度；
- 当前 Renderer 基础已到 S36.2，但 Face SDF、真实 Toon Ramp、Hair KK、Rim、材质分离、
  Emissive/Bloom、Outline 和 Lighting/Grade 的最终画面仍未达到作品集标准；
- FYP 同时包含作品集产品与三路径研究，必须通过稳定依赖关系避免互相污染。

本次完成：

- 新建仓库级 `MASTER_DEVELOPMENT_PLAN_CN.md`；
- 生成并逐页检查 `FYP_Master_Development_Plan_v2.0.docx`；
- 冻结 M0–M11 路线：角色质量、工业场景、作品集发布、Benchmark Freeze、Multi-pass、
  Subpass、DRLR、Android、实验、论文与最终提交；
- 建立统一状态、Exit Gate、Must/Should/Could、决策登记、风险登记和证据规则；
- 将 Portfolio Renderer 与 Benchmark Core 明确分离，三条研究路径只允许改变 Render
  Organization 与 Attachment Access Route；
- 当前唯一 Active 节点改为 M1/CQ-0 固定视觉 QA Harness。

固定执行顺序：

`CQ-0 -> CQ-1 -> CQ-2 -> CQ-3 -> CQ-4 -> CQ-5 -> CQ-6 -> M2 Gate`

延期项：

- S36.3 Exposure/Tone Mapping 交互与正式性能；
- Subpass、DRLR、Android；
- 大型场景或与主要目标无关的新系统。

下一节点：

- 建立 CQ-0 的五类固定机位、四类灯光环境、效果 Isolation View、Enabled/Disabled A/B
  Capture 与 Manifest；
- 后续所有角色 Shader 节点必须通过同一 QA Harness 验收。

## M1/CQ-0 — 固定角色视觉 QA Harness

日期：2026-08-02

本次完成：

- 新增五类确定性 QA Camera：Full Body Front、Face Front、Face 3/4、Back Detail、Lighting Sweep；
- 新增 Neutral Material、Stylized Key、Specular/Rim、Rear Emissive 四类灯光环境；
- 新增 Beauty、Albedo、World Normal、Depth、Diffuse Band、Shadow Visibility、Hair KK、Rim、Specular、Emissive、Outline、Shadow Map 共 12 类视图；
- 为 Toon、Shadow、Hair KK、Rim、Specular、Emissive、Outline 建立 Enabled/Disabled/Isolation 三态 A/B；
- 普通分量 Isolation 会关闭倒壳与内部轮廓，Outline A/B 同时控制两条轮廓路径；
- Capture Manifest 记录 QA 状态与 FNV-1a-64 状态哈希；批量索引记录资产、可执行文件和 12 个 SPIR-V 的 SHA-256；
- 新增可恢复批次脚本以及通用 QA/Reference Contact Sheet 工具。

验证与证据：

- Ninja Debug 与 Release 构建成功；
- `captures/cq0_public_smoke_v2`：公共资产两帧 Debug Validation，退出码 0；
- `captures/cq0_laevat_baseline_v2`：20 Case，四个 Lighting Sweep 各 60 帧；
- `captures/cq0_laevat_isolation_v2`：12 Case；
- `captures/cq0_laevat_ab_v1`：21 Case；
- `captures/cq0_release_representative_v1`：私有角色 Release 代表帧；
- `captures/cq0_review_v1/current_reference_isolation.png`：Reference/Current/Isolation 三列人工评审图；
- 所有 Debug 批次未报告 Validation Warning/Error，Manifest 与索引均成功解析。

视觉审计：

- Specular、Rim、Outline 的 A/B 差异可见；
- Toon 与 Hair KK 差异过弱，不能视为作品集级完成；
- Emissive 仅集中在少量红色发光部件，尚无 Bloom 与亮度层级系统；
- Depth 诊断接近二值，Material ID 和 Face SDF 尚不存在；
- 以上缺口已分别进入 CQ-1～CQ-6，CQ-0 不伪造结果。

1280×720 全图 A/B 首帧量化（Mean Absolute RGB Difference / Changed Pixels）：Toon 0.8091 / 12.986%，Shadow 0.1101 / 2.836%，Hair KK 0.0142 / 1.597%，Rim 1.6797 / 4.365%，Specular 6.0733 / 23.056%，Emissive 0.0277 / 0.244%，Outline 1.0557 / 4.260%。这些数字只证明效果开关是否产生可复现差异，不等价于美术质量评分。

结论：CQ-0 Complete。当前唯一 Active Work Package 切换为 CQ-1 Material Classes/Data v1；下一步先做资产/材质用途审计与版本化分类，不提前实现 Ramp、Face SDF 或 Hair KK。

## M1/CQ-1 — Material Class / Data v1

日期：2026-08-02

资产审计：

- 莱万汀源 GLB 实际为 13 个源材质、13 个角色 Primitive；Fallback 与运行时地台使最终统计成为 15 材质、14 Primitive；
- 分类为 Skin 2、Face 1、Hair 1、Fabric 5、Eye 1、Overlay 3；当前没有独立 Metal 或 Emissive Primitive，金属/发光区域存在于 Cloth 复合纹理；
- 不再根据材质序号硬编码分类。

实现：

- 新增 Material Class v1：Generic/Skin/Face/Hair/Fabric/Metal/Eye/Overlay/Emissive/Showcase；
- 新增 Stylized Shadow、Hair Anisotropy、Face SDF Eligible、Emissive Mask、Overlay、Neutral Fallback Feature Flags；
- glTF `extras.azureRenderMaterial` 保存 Schema Version、Class、Flags、`styleParameters` 和 `featureParameters`；
- 新增 `schemas/azure_render_material.schema.json` 与无第三方依赖的 `tools/validate_material_profiles.py`；
- Push Constant 从 80 bytes 扩展至 Vulkan 最低保证上限 128 bytes；后续参数不得继续塞入 Push Constant；
- Shader 使用 Feature Flags 限制 Hair、Face Matcap、Emissive 与 Shadow Tint 的材质所有权；
- 新增 `material-id` Isolation、启动材质清单、HUD Class 数量/Face-Hair 关键参数，以及 Manifest `materialInventory`；
- 旧私有资产可使用明确标记的名称推断；未知公共资产进入 Generic Neutral Fallback。

验证：

- 13 个显式 Profile 全部通过 Schema 字段、枚举、唯一性和范围验证；
- Debug/Release 构建成功；
- `captures/cq1_laevat_isolation_v1` 完成 13 个 Isolation Case，无 Validation Error；
- `captures/cq1_material_id_face_v2` 显示 Face/Hair/Skin/Fabric/Eye/Overlay 边界；
- `captures/cq1_material_hud_v1` 显示 Class 数量和 Face/Hair 参数；
- `captures/cq1_public_fallback_v1` 验证公共资产全部使用 Generic Fallback；
- `captures/cq1_release_beauty_v1` 完成 Release 代表帧；
- 在参数介入前的 CQ-1 结构回归与 CQ-0 基线 SHA-256 完全一致；启用显式分类参数后的变化只属于预期材质分离，并完成近景视觉检查。

结论：CQ-1 Complete。当前 Active Work Package 切换为 CQ-2 Toon Ramp/Shadow v1；下一步只实现可编辑 Ramp 与阴影层级，不提前进入 Face SDF、Hair KK、Bloom 或最终调色。

## M1/CQ-2 — Toon Ramp / Shadow v1

日期：2026-08-13

实现：

- 新增版本化 `assets_public/toon_ramp_profiles.json` 与生成的 10x64 线性 PPM Atlas；
- `linear` Ramp 用于 Skin/Face，`step` Ramp 用于 Hair/Fabric/Metal/Eye；
- 新增 Descriptor binding 11，使用 Renderer-owned UNORM Ramp Texture；
- 保持 128-byte Material Push Constant 不变；
- 删除主光照对全局 `smoothstep(N dot L)` 的依赖，Material Class 选择 Ramp 行；
- 分离 Direct Diffuse、Ambient、Shadow Map Visibility、AO 与 Lam Shadow Tint；
- Style Mask 改为控制 Ramp 坐标、Shadow/AO 和 Specular 权重，不再只叠加弱 Accent；
- Toon 单项关闭只关闭 Ramp，保留 Rim、Hair KK、Matcap 等其他效果；
- 新增 `style-mask`、`ambient`、`direct-diffuse`、`shadow-tint` Isolation；
- Manifest 与 QA Index 保存 Ramp Profile/Atlas Hash；
- Background 与 Inverted-Hull Outline 显式写 Normal Attachment，消除两条 Validation warning；
- CMake 正式发现 stb include path，Linux 系统包布局可编译。

资产与验证：

- 从 `laevat_skinned.glb` 生成忽略提交的 `laevat_skinned_material_cq2_v1.glb`；
- 13 个源材质全部通过显式 Material Profile v1 验证；
- Linux Debug/Release 构建成功，12 个 GLSL Shader 全部由 glslc 编译；
- NVIDIA GeForce RTX 5070 Ti Laptop GPU 上公共资产 Debug Validation 120 帧通过；
- 私有角色 Debug Validation 与 Release 各 120 帧通过，468 Joint Matrix 正常；
- 最终 60 帧 Lighting Sweep 通过，无 Validation warning/error；
- 1920x1080 Release Beauty 位于 `captures/cq2_release_beauty_v2`；
- 评审图位于 `captures/cq2_review_v1/cq2_review_sheet.png`；
- Toon A/B：Mean Absolute RGB Difference 0.981119，Changed Pixels 18.503255%；
- 所有代表 Capture Alpha 非 255 像素数为 0；
- Release GPU Timestamp 功能探针：Shadow 0.102 ms、Main 0.264 ms、Final 0.024 ms、Total 0.390 ms；该数据不作为正式性能结论。

结论：CQ-2 Complete。当前 Active Work Package 切换为 CQ-3 Face SDF 与脸部 Overlay；不提前进入最终 Hair KK、Bloom 或最终调色。

## M1/CQ-3 + AR-0 — Face SDF 契约与 RenderSettings 基线

日期：2026-08-13

目标：在实现 Face SDF Shader 前先冻结输入契约和外部控制边界，避免 CLI、未来 GUI、
Capture 与 Renderer 各自维护一套参数，也避免把普通脸部贴图误判为 SDF。

实现：

- 新增 `src/render/RenderSettings.*`，收口 Stylized Lighting、Style Mask、Ramp
  Threshold、Showcase Preset、Outline、Diagnostic View 和 Face SDF v1 参数；
- `AzureRenderOptions`、键盘控制、UBO、HUD、Command Recording 与 Capture Manifest
  改为读写同一 `RenderSettings`；
- Manifest 新增 `renderSettingsVersion` 和完整 `faceSdfSettings`，并纳入 QA State Hash；
- Material Profile Schema 增加可选 `faceSdf`：Texture、UV Set、Channel、值域方向、
  Horizontal Axis 与 Head Node；
- Loader 严格解析该配置，保存 SDF 像素与 Head Node 索引；错误索引、通道、UV 或节点
  在资产加载阶段失败；无 `faceSdf` 的旧资产保持兼容；
- 新增 `tools/audit_face_sdf_compatibility.py`，报告 Face Primitive、UV、纹理来源、
  Head Node 和具体不兼容原因；
- 新增 `docs/RENDERER_MODULARIZATION_PLAN_CN.md`，冻结 AR-0 至 AR-4 的增量边界。

资产结论：

- 公共 `test_model.gltf` 没有 Face Material，不能作为 Face SDF 验证资产；
- 莱万汀 `M_actor_laevat_face_01` 有一个 Face Primitive、`TEXCOORD_0` 和
  `face-sdf-eligible`；
- 当前私有 GLB 没有 `azureRenderMaterial.faceSdf`，也没有可审计的来源、通道、方向和
  Head Node 绑定；
- 决策：不复用 `T_actor_laevat_face_01_D` 等普通贴图，制作 AzureRender 自有 Face SDF。

验证：

- Material Profile v1 的 13 个私有材质继续通过 Schema 验证；
- Linux Debug/Release 重新配置并构建成功；
- Python 审计/验证工具通过语法检查；
- 公共资产 Debug Validation、私有资产 Debug Validation 与私有 Release 各运行
  120 帧，均退出码 0，Debug 未报告 Validation warning/error；
- 公共资产 640x360 单帧 Capture 成功，Manifest JSON 合法且包含
  `renderSettingsVersion: 1` 与完整 `faceSdfSettings`；
- `git diff --check` 通过。

结论：AR-0 的设置与资产契约增量完成；CQ-3 仍为 Active。下一步制作自有 Face SDF，
绑定明确 Head Node，并实现 Head-local 光照、左右翻转、Threshold/Softness 与 Face SDF
Isolation。之后再开始 AR-1 Renderer Core Boundary，不提前进入完整 Editor/ECS。
## 2026-08-13 CQ-3 Face SDF v1

- 建立 Face SDF v1 资产契约，生成 `assets_public/face_sdf_v1.png` 并注入私有 CQ-3 GLB。
- Loader 解析并规范化距离/遮罩通道；运行时接入 binding 12、Head-local 光向量和 Face 漫反射分带。
- 新增 `face-sdf` QA effect/isolation，契约校验与兼容性审计通过。
- Debug Shader/C++ 增量构建通过；Vulkan/Xvfb 视觉回归受当前执行环境 loopback 初始化错误阻塞，节点保持 Review。
