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
