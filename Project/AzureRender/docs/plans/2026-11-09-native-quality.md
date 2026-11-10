# 原生画质与正确性实施计划

日期：2026-11-09。
状态：自动执行中。
工作树为 `D:/Projects/FYP-worktrees/nr-quality`。
分支为 `codex/nr-quality`，基点为 `3f9c745`。

## 目标与边界

完成原生接收精度、头发及局部外观核验。
原生实际 GPU 附件持有数值判据。
公共对象过滤器属于渲染模块。
单页对象与多对象图集共享过滤契约。

输入、程序和着色器以 SHA-256 冻结。
输入复用须具有主维护索引租约。
GPU 运行须持有统一队列独占权。
共享着色器与根构建清单由集成负责人汇合。

## 验收条件

接收区域采用帧 10401 的 49384 个有效像素。
可见量最大字节误差至多为一。
完整头发采用既有 17017 像素遮罩。
每个颜色通道的误差至多为一字节。

局部通过后核对原四帧的 RGB 零差。
静态有效区域的 SSIM 至少为 0.95。
关键区域中位亮度误差至多为 10%。
双设备、动态与人工条件按原门禁验收。

## 接收精度

- [x] 核对交接、基点、所有权与空间预算。
- [x] 建立独立稀疏工作树并登记任务。
- [x] 取得输入租约及统一运行入口。
- [x] 组合精确重建与生产固定采样足迹。
- [x] 对 Intel 和 NVIDIA 执行区域核对。
- [x] 记录齐次坐标差异与可见量差异。
- [x] 根据实际误差形成单因素候选。
- [x] 验证主配置所属回归并封存公共通道证据。
- [x] 集成公共通道，完成双配置与原四帧回归。

候选引用 `native-producer-gpu-intermediates.md`。
过滤契约引用 `shadow-gather-footprint.md`。
诊断输出写入本工作树的受管任务。
单元输出预算为 64 MiB，探索预算为三十分钟。

组合结果见[专项验收](../acceptance/nr/native-quality-combination.md)。
Intel 保留 120 处超差，最大为 33 字节。
后续候选依赖世界位置算术的新证据。

[倒数规则](../acceptance/nr/native-reciprocal-domain.md)完成实际输出对照。
私有完整域查表在 Intel 保留一处超差，最大四字节。
原生齐次输入经查表可逐位重建全部世界位置。
剩余候选核对齐次 X 的精确乘加边界。

[视口归一化精度](../acceptance/nr/receiver-exact-viewport.md)通过双设备参考核对。
49384 个世界位置逐位一致，可见量超差为零。
独立的 65536 组整数舍入用例在两设备全部一致。
后续压缩倒数参考资源，并接入公共生产契约。
[两位残差配置](../acceptance/nr/reciprocal-compact-profile.md)完成双设备完整域核对。
8388608 项逐位一致，配置占 2 MiB。
公共接入继续核对版本、输入范围及资源生命周期。

## 公共数值配置

配置归属 `ScreenObjectVisibilityGpu` 的构造契约。
设备算术为默认模式，参考模式版本为 `ProfileV1`。
参考配置包含固定的 524288 个残差字。
实例复制输入，并拥有不可变的存储缓冲。

构造前校验模式、长度、保留编码及资源预算。
片元缓冲数量和容量通过 RHI 能力入口查询。
绑定五使用逐槽描述符，缓冲由实例共享。
调用方等待在途工作结束后销毁实例。

参考算术覆盖视口归一化及规范化倒数映射。
倒数结果为非规格化值时采用设备除法。
默认模式的占位缓冲为四字节。
参考模式的配置缓冲为 2 MiB。

独立工具与原生校准夹具共同核对合法工作流。
契约检查覆盖构造中断、拒绝状态及槽位复用。
公共助手另做整数舍入及完整尾数范围核对。
主场景集成、四帧与完整颜色沿原门槛推进。
实现与最终核对见[参考算术专项](../acceptance/nr/visibility-reference-arithmetic.md)。
主配置的十四项双设备 GPU 检查全部通过。
两份原生区域均满足一字节门槛。
[主线集成](../acceptance/nr/visibility-reference-integration.md)通过两配置共 32 项有效检查。
原始四帧的 RGB 与 PNG 摘要均保持零差。
当前场景深度和完整颜色继续分别验收。

## 完整头发颜色

- [x] 复现冻结区域的 73 处超差及六字节峰值。
- [x] 将误差位置关联到合格的实际 GPU 输入。
- [x] 对有证据的单因素候选执行完整区域核对。
- [ ] 通过后执行受影响材质及原四帧回归。
- [ ] 同步专项验收与总计划，提交独立交付。

基线见 `../acceptance/nr/hair-gradient-region.md`。
采样依据见 `../acceptance/nr/exact-hair-sampling-inputs.md`。
生产候选须通过完整区域门禁。

本轮归因见[头发颜色与可见量](../acceptance/nr/hair-color-visibility-attribution.md)。
42 个颜色失败点同时具有片元可见量超差。
来源替换控制保留 31 处颜色超差，最大为六字节。
42 处原有颜色失败点进入门槛，完整生产接入待验。
[基色层级内容](../acceptance/nr/hair-base-color-mip-content.md)完成十二级 RGB 核对。
5592405 个纹素的 RGB 逐通道一致。
[实际基色取样](../acceptance/nr/hair-base-sample-region.md)完成 31 个剩余位置的全位核对。
27 个位置逐位一致，四处取样差异继续定位。
[参数及双法线](../acceptance/nr/hair-material-sample-region.md)完成 48 个版本的资格核对。
参数图有 25 处逐位一致，六处取样差异继续定位。
[实际 UV](../acceptance/nr/hair-uv-sample-region.md)核对完成。
六处材质差异中，五处存在 UV 差异，一处保持原层级边界。
[插值法线与切线](../acceptance/nr/hair-interpolated-basis.md)完成 42 个探针的资格核对。
参数图相同的 25 个点仍有几何基底差异。
后续以实际输入替换控制核定颜色贡献。
[基底颜色控制](../acceptance/nr/hair-basis-color-control.md)通过激活及恒等资格。
原生法线只闭合一处颜色失败，仍有 30 处超差。
后续核对附件转换前的实际浮点输出。
[附件前浮点输出](../acceptance/nr/hair-final-float-values.md)完成双端存储资格核对。
格式映射使六个位置与原生存储值一致，其他位置具有着色差异。
后续按色度线索核对漫反射及条纹调制。
[漫反射色带](../acceptance/nr/hair-diffuse-ramp-input.md)完成 36 个探针核对。
两端纹理字节一致，主要色度组的 19 个取样逐位相同。
[响应权重](../acceptance/nr/hair-diffuse-response-weights.md)完成 36 个探针核对。
主要色度组的 19 个遮罩逐位相同。
[条纹调制](../acceptance/nr/hair-stripe-modulation.md)完成 36 个探针核对。
主要色度组的系数峰差约为 0.0000003576。
[主光色度运算](../acceptance/nr/hair-main-chroma-operation.md)完成实际取值与单因素控制。
主要色度组的最终峰差在诊断控制下为 0.0000861287。
首个 Vulkan 合成范围候选未通过完整区域门禁。
[所选主灯组合](../acceptance/nr/hair-selected-main-composite.md)完成实际输入及完整区域核对。
候选闭合 17 处超差，保留 14 处且没有新增超差。
[公共主光色度](../acceptance/nr/hair-main-brightness-saturation.md)完成两配置、双设备接入核对。
每组 317 项数值、共 28 个图像及默认原四帧通过。
[修正后的浮点输出](../acceptance/nr/hair-main-chroma-final-float.md)通过九项探针资格。
31 处半精度存储重建合格，26 处 R11 存储预测一致。
[R11 存储控制](../acceptance/nr/hair-r11-storage-control.md)的整区 HDR 符合转换模型。
实际最终颜色保留六处超差，包含一处新增超差。
候选保持隔离。
[显示消费边界](../acceptance/nr/hair-display-consumption.md)的整区比较最大差为一字节。
[参数与双法线层级](../acceptance/nr/hair-normal-param-mip-content.md)的二十四级内容全部一致。
[顶点投影来源](../acceptance/nr/hair-postvs-clip-source.md)已核对实际缓冲及原生投影运算。
11564 个原生裁剪向量逐位重建。
[相对投影来源控制](../acceptance/nr/hair-relative-projection-control.md)使六处超差保留两处。
四处进入原门槛，控制未新增超差。
[细粒度梯度组合](../acceptance/nr/hair-relative-fine-gradient-control.md)保留一处新增超差。
[四像素组梯度组合](../acceptance/nr/hair-relative-quad-gradient-control.md)通过私有来源完整区域。
17017 个像素的峰差为一字节，原 73 处全部闭合且新增为零。
公共取样、投影、存储精度及实际深度继续接入验收。

## 后续外观

- [ ] 按原计划核对皮肤、眼睛、GI 与反射。
- [ ] 核对动态衣物及六视图的适用输入。
- [ ] 完成所有可执行回归并汇总剩余条件。

执行顺序与原门禁见 `azure-engine-plan.md`。
硬依赖以具体输入、负责人和重试条件登记。
整体完成状态由全部适用门禁共同决定。

## 公共输出精度

目标为可选的无符号低范围浮点颜色输出。
材质通过版本二的 `r11g11b10-output` 特性选择。
特性占第 26 位，默认材质保持原始输出。
材质表继续采用 22 行及 352 字节。

所属模块为资产契约与公共片元颜色助手。
合法输入为受光的不透明或遮罩表面。
独立 PBR 夹具与投影头发共同验证复用。
透明合成及无光照表面显式拒绝该特性。

### 文件与接口

`src/assets/GltfLoader.hpp` 声明特性位。
`src/assets/MaterialOutputPrecision.hpp` 校验所有权并提供 CPU 参考。
`GltfLoader.cpp` 与 `MaterialSurfaceProfile.hpp` 共用校验。
`shaders/material_output_precision.glsl` 提供公共转换。

`mesh.frag` 在最终颜色写入后应用转换。
RGB 采用五位指数及六、六、五位尾数。
转换向零舍入，有限溢出饱和到最大有限值。
负数归零，NaN 与正无穷保留各自类别，Alpha 按位保留。

`tests/MaterialOutputPrecisionGpuTests.cpp` 对照独立数值参考。
`shaders/material_output_precision_test.comp` 逐位读取结果。
根构建清单注册夹具和所属着色程序。
材质模式及完整文档重开由角色契约测试覆盖。

### 执行检查

- [x] 先写 GPU 及配置失败检查，确认缺失接口。
- [x] 实现合法所有权、边界数值及公共片元出口。
- [x] 核对全部半精度输入及分层单精度输入。
- [x] 在双设备、两配置执行所属检查。
- [x] 核对独立 PBR、头发及默认原始四帧。
- [x] 同步模式、运行说明、证据和独立提交。

非有限 RGB、负零和最小非零值单独核对。
透明模式、版本一和无光照组合必须提供明确错误。
默认输出和 Alpha 的位值保持原契约。
生产深度、摄像机与梯度另有后续验收单元。

[公共输出精度验收](../acceptance/nr/material-output-precision.md)已完成双配置、双设备检查。
十六张公开材质图与原始四帧通过，默认画面保持一致。
公共梯度、相对投影及实际深度继续接入。

## 公共 UV 梯度助手

本单元提供片元四像素组的 UV 梯度助手。
策略类型为 `MaterialUvGradientPolicy`。
设备默认策略无需四像素组能力。
纵向镜像粗梯度策略要求片元子组及四像素组操作。

`src/render/MaterialUvGradients.hpp` 校验策略及支持条件。
`shaders/material_uv_gradients.glsl` 提供统一的梯度计算。
调用方在一致控制流中、丢弃片元前计算梯度。
后续纹理取样接收已计算的梯度。

`tests/MaterialUvGradientsGpuTests.cpp` 使用真实图形管线。
测试覆盖偶数及奇数尺寸、片元丢弃与资源释放。
原始四帧按实际宿主及既有着色程序的相同摘要复用。
材质绑定、覆盖及阴影取样在后续生产接入单元验收。

- [x] 先确认公共接口及着色助手的缺失失败。
- [x] 实现策略校验、设备查询及公共 GLSL 助手。
- [x] 两配置、双设备逐位核对实际光栅结果。
- [x] 同步说明、证据和独立提交，继续材质绑定。

[公共 UV 梯度助手](../acceptance/nr/material-uv-gradients.md)通过四批实际光栅核对。
斜边三角形的覆盖外辅助调用通过双配置、双设备补验。
69728 个分量逐位一致，原有宿主及着色程序摘要保持一致。
材质绑定及各覆盖路径继续在后续单元接入。

[主 UV 取样候选](../acceptance/nr/material-mirrored-uv-candidate.md)保留 Intel 阴影差异。
候选接入待验，相机投影与真实深度来源继续推进。

[参考裁剪投影](../acceptance/nr/reference-clip-projection.md)通过原生向量与双设备位精度核对。
公共内核与原四帧回归通过，相机及深度生产继续接入。

[参考几何深度](../acceptance/nr/reference-depth-source.md)通过双配置、双设备及原四帧回归。
公共不透明生产通过，原生覆盖、壳层和可见量继续接入。

[几何深度与可见量](../acceptance/nr/geometry-depth-visibility.md)完成双设备成对核对。
原生深度控制通过，几何来源的七处及一百零三处超差继续修正。

[深度透明覆盖](../acceptance/nr/reference-depth-coverage.md)通过公共表面及默认四帧检查。
NVIDIA 完整原生深度与可见量精确一致，Intel 九十七处超差继续修正。

[Intel 深度与透明取样来源](../acceptance/nr/intel-depth-alpha-footprint.md)完成位精度控制。
九十七处超差分为七十六处单末位、二十处较大深度差及一处透明判定差。

[实际顶点裁剪读回](../acceptance/nr/actual-vertex-clip-readback.md)通过双设备位精度核对。
每设备四万三千零四十四个向量精确一致，后续核对视口及光栅插值。

[光栅图元与深度模型](../acceptance/nr/raster-primitive-depth-model.md)完成双设备来源核对。
九十六处超差来自相同图元，透明层级及插值数值继续修正。
