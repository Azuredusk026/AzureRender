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
- [ ] 对有证据的单因素候选执行完整区域核对。
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
后续核对实际顶点、插值与梯度来源。

## 后续外观

- [ ] 按原计划核对皮肤、眼睛、GI 与反射。
- [ ] 核对动态衣物及六视图的适用输入。
- [ ] 完成所有可执行回归并汇总剩余条件。

执行顺序与原门禁见 `azure-engine-plan.md`。
硬依赖以具体输入、负责人和重试条件登记。
整体完成状态由全部适用门禁共同决定。
