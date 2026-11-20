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

[参考图集光栅](../acceptance/nr/reference-object-shadow-raster.md)核对实际图集来源。
[材质参考对象可见量](../acceptance/nr/material-reference-object-visibility.md)核对同帧颜色。
NVIDIA 的 17,017 个头发可见量和颜色位置均满足一字节门槛。
Intel 的冻结可见量和颜色分别核对设备输入。

[Intel 参考可见量来源](../acceptance/nr/intel-reference-visibility-sources.md)保存固定输入和原生选级读回。
同输入的 `ProfileV1` 达到接收门槛。

[项目参考可见量算术](../acceptance/nr/project-reference-visibility-profile.md)核对编辑器、Player 和发布配置。
完整接收分别核对事件 377 的法线与事件 422 的深度。
Intel 头发颜色继续核对纹理梯度与层级选择。
[原生头发显式层级](../acceptance/nr/evidence/2026-11-15/native-hair-hardware-explicit-levels/results.json)在两块显卡上核对五个相同图元位置。两处基色及法线层级不同，另外两处层级一致并保留颜色差异。

[原生接收输入来源](../acceptance/nr/native-receiver-producer-replay.md)记录服饰双通道法线和头发外扩写入。
参考场景外扩深度与服饰法线分别进入生产验证。

[参考头发近邻层级](../acceptance/nr/reference-hair-mip-boundary.md)完成双配置、双设备和原始四帧回归。NVIDIA 的冻结头发可见量及 RGB 全部满足一字节门槛。Intel 分别有 111 处和 568 处超差，完整接收有 494 处超差。接收附件和着色阶段的纹理采样继续按原门槛核对。

[投影头发主纹理层级](../acceptance/nr/projected-hair-point-mip.md)完成材质接入及生产回归。NVIDIA 冻结 RGB 逐通道满足一字节门槛，Intel 有 32 处超差。剩余颜色、参考可见量和体表法线按实际光栅输入继续核对。

[头发光栅量化与体表法线](../acceptance/nr/reference-raster-source.md)完成两项双设备来源对照。量化对照定位到顶点精度传播，后续隔离量化运算。体表对照确认采样后法线重建的影响，接收深度、法线和图集继续独立核对。

[量化精度隔离与接收输入](../acceptance/nr/reference-raster-isolated.md)核对双设备整帧，Intel 冻结 RGB 有 31 处超差，区域外变化均在一字节内。原生深度、法线和图集的联合输入通过接收门槛，生产输入及共同图元继续验收。

[投影头发参考光栅网格](../acceptance/nr/projected-hair-reference-grid.md)完成可选材质路径的回归。
全身虹膜和生产接收继续按各自门槛验收。

[虹膜覆盖来源](../acceptance/nr/iris-native-overlay.md)核对原生覆盖对 94 个虹膜位置的影响。
头部平面主光下的双设备配对采集确认实际眼影覆盖与原生一致，最终虹膜仍分别有 22 处和 30 处超过一字节。

[角色动态重开](../acceptance/nr/evidence/2026-11-15/current-joint-character-reopen/results.json)在 NVIDIA 595.79 下验证已保存场景、11 个动画实例及四个连续采集时点。四帧对旧驱动保留画面分别有 28、48、56、62 处超过一字节，原生连续画面验收保持开放。

[固定姿态眼部阴影几何](../acceptance/nr/iris-overlay-geometry.md)通过双设备虹膜区域门槛，逐像素遮挡与乘色继续验收。
[虹膜反射贴图梯度](../acceptance/nr/evidence/2026-11-15/iris-vulkan-matcap-gradients/results.json)核对 255 个 Vulkan 样本及原生层级。两种梯度分别在不同位置匹配原生第 7 级，眼部完整颜色继续验收。

[眼部阴影纹理与遮挡](../acceptance/nr/iris-overlay-source.md)核对原生纹理、乘色及完整几何覆盖。

[体表法线原生选层](../acceptance/nr/native-body-explicit-mips.md)核对体表实际选层与解码。
生产可见量及共同图元按原门槛继续验收。

[体表接收法线小数精度](../acceptance/nr/body-fractional-normal.md)核对生产 Player 及新增可见量超差。
[体表接收分项](../acceptance/nr/evidence/2026-11-15/body-receiver-operands/results.json)复算 9,116 处深度、法线和图集的作用。原生三项输入在两设备上均进入一字节门槛，生产接收保持待通过。

[体表固定层级与图集纹素](../acceptance/nr/evidence/2026-11-15/body-receiver-normal-and-atlas/results.json)核对 11 层法线和 5 个图集纹素。原生头发写入其中 4 个，衣料写入 1 个。原生片元深度及 Vulkan 图集硬件读回确认五处相差一个 D16 单位。

[虹膜原生硬件梯度](../acceptance/nr/evidence/2026-11-15/iris-native-derivative-lanes/results.json)读回 255 处采样导数。粗粒度计算分别匹配 NVIDIA 的 247 处和 Intel 的全部 255 处。
[脸部染色与固定环境光](../acceptance/nr/evidence/2026-11-15/face-tint-and-fixed-ambient/results.json)核对六处原生脸部片元和对应照明输入。中庭体积辐照继续按独立场景门槛验收。

[正式遮罩与最近深度发影](../acceptance/nr/evidence/2026-11-15/face-public-mask-and-nearest-shade/results.json)的双设备联合复测各有 34 处脸部像素变化，完整脸部新增超差及脸部范围外变化均为零。两块显卡共有 49 处超过八个色阶的不透明脸缘像素，后续按原生绘制 804 核对。

[冻结四帧差异位置](../acceptance/nr/evidence/2026-11-15/frozen-cross-driver-spatial/results.json)记录天花板区域及高差位置。原始四帧仍按 RGB 零差验收。

[体表描边视口原点](../acceptance/nr/evidence/2026-11-15/body-outline-window-origin/results.json)复现 105 处原生整数取样地址。

[描边与透明衣料提交顺序](../acceptance/nr/evidence/2026-11-15/body-outline-before-transparent-cloth/results.json)完成双设备复测，衣料覆盖区超过八个色阶的像素均由 56 降至 5。覆盖区外逐像素一致。

[原生虹膜层级足迹](../acceptance/nr/evidence/2026-11-15/iris-native-mip-footprints/results.json)按设备的硬件匹配范围复算双设备各 255 处。对角足迹与 NVIDIA 的 255 处 Vulkan 层级查询一致，Player 严格眼部验收继续进行。

[虹膜透视梯度对照](../acceptance/nr/evidence/2026-11-15/iris-projective-bare/results.json)在关闭眼影的固定姿态确认双设备接缝误差降至一个色阶，虹膜外逐像素一致。眼影开启与基色贴图选层继续按同一冻结参考复测。

## 后续外观

- [ ] 按原计划核对皮肤、眼睛、GI 与反射。
- [ ] 核对动态衣物及六视图的适用输入。
- [ ] 完成所有可执行回归并汇总剩余条件。

执行顺序与原门禁见 `azure-engine-plan.md`。
硬依赖以具体输入、负责人和重试条件登记。
整体完成状态由全部适用门禁共同决定。

[原生地面画面对照](../acceptance/nr/evidence/2026-11-16/joint-scene-native-floor/results.json)核对原生第 3534 帧、辐照体开关和两种屏幕反射路径。中央地面与青色环带继续按 0.95 的门槛验收。
[投影反射射线步数](../acceptance/nr/scene-material-quality-diagnosis.md#联合场景步数对照)登记 32 步候选的双设备实际帧。中央地面中位亮度及邻区新增超差未通过，反射按历史取样坐标、深度和颜色继续验收。
[原始反射颜色](../acceptance/nr/evidence/2026-11-16/projected-reflection-shared-hits/results.json)按深度配准区分命中覆盖与共同命中处的颜色误差，用于选择地面反射的后续修复。
[区域历史取样](../acceptance/nr/evidence/2026-11-20/reflection-regional-cross-address/README.md)复算两区的历史地址和时间增量。最近深度对照记录单帧采用判定。
[半分辨率深度对照](../acceptance/nr/evidence/2026-11-20/reflection-nearest-depth-candidate/README.md)核对搜索预算及同址深度。
[接收法线与前景深度](../acceptance/nr/evidence/2026-11-20/reflection-surface-normal-lineage/README.md)定位大厅命中长尾及参考输入条件。

角色固定姿态按[脸部与体表区域验收](../acceptance/nr/face-skin-color-sources.md)记录纹理、照明、描边和透明衣料的来源。
[固定姿态网格与轮廓](../acceptance/nr/evidence/2026-11-15/fixed-pose-face-and-body-geometry/results.json)核对身体顶点、脸部索引和同机位的覆盖复算。固定姿态的剩余色差及六视图按各自门槛验收。
[虹膜眼影覆盖](../acceptance/nr/evidence/2026-11-16/iris-projective-covered/results.json)及[原生体表法线取样](../acceptance/nr/evidence/2026-11-16/native-body-normal-raw-samples/results.json)完成双设备实测，完整颜色及可见量继续按原门槛验收。
[固定姿态材质对照](../acceptance/nr/evidence/2026-11-16/fixed-pose-material-controls/results.json)保存身体主光、衣料和脸部漫反射的实机结果。生产颜色门槛包含可见区域与邻区的新增超差。
[双层衣料与覆盖](../acceptance/nr/evidence/2026-11-16/native-two-cloth-overlay-controls/results.json)核对两个原生绘制与定向衣料合成，覆盖及新增误差按原门槛验收。
[动态角色网格](../acceptance/nr/full-body-native-comparison.md#动态角色网格)记录身体与脸部的原生索引和静止顶点对照。身体候选按动态轮廓、接缝及双设备画面验收。
[完整皮肤区域对照](../acceptance/nr/evidence/2026-11-15/body-skin-coverage-composition/results.json)按原生绘制归属核对 47,458 个位置。描边局部光通过双设备区域结构相似度和中位亮度门槛，完整颜色继续按一字节门槛验收。
[描边径向参考点](../acceptance/nr/evidence/2026-11-15/body-outline-native-rim-origin/results.json)通过双设备姿态实测，覆盖之外逐像素一致。描边保留区在 NVIDIA 和 Intel 上分别有 48 和 49 处超过八个色阶。

[描边阴影查找表](../acceptance/nr/evidence/2026-11-16/body-outline-native-shadow-response/results.json)核对原生纹理与四个片元的漫反射响应。两设备分别有 20 和 15 处新增超差，完整画面继续按原门槛验收。

[第一层衣料描边网格](../acceptance/nr/evidence/2026-11-15/native-cloth-one-outline-topology/results.json)核对原生绘制 1079 的直接位置、投影偏移和三角形对应关系。20 个皮肤像素与待核对三角形相交，按原生片元继续复测。

[衣料覆盖照明](../acceptance/nr/evidence/2026-11-15/body-late-cloth-native-components/results.json)核对原生四个硬件片元。两处峰差的原生红通道以主光漫反射为主，独立衣料夹具与描边配置继续联合复测。

[脸部正式材质对照](../acceptance/nr/evidence/2026-11-15/public-face-receiver-depth-and-mask/results.json)核对距离场遮罩选层、发影最近片元乘色与同驱动默认四帧。全脸一字节门槛和原始冻结参考保持开放。
脸部按主光饱和度、视角染色与局部灯光分项核对。
眼周及发影覆盖按接收类别、遮挡和线性颜色分项核对。
近景眼部按[乘色与泛光对照](../acceptance/nr/eye-multiply-receiver.md)核对覆盖及颜色。
冻结四帧保留 592.82 参考，现用驱动的同环境回归单独记录。

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
材质绑定、覆盖及阴影取样见公开主 UV 绑定验收。

- [x] 先确认公共接口及着色助手的缺失失败。
- [x] 实现策略校验、设备查询及公共 GLSL 助手。
- [x] 两配置、双设备逐位核对实际光栅结果。
- [x] 同步说明、证据和独立提交，继续材质绑定。

[公共 UV 梯度助手](../acceptance/nr/material-uv-gradients.md)通过四批实际光栅核对。
斜边三角形的覆盖外辅助调用通过双配置、双设备补验。
139456 个分量逐位一致，原有宿主及着色程序摘要保持一致。
公开主 UV 绑定及所属覆盖路径通过三十六组检查。

[主 UV 取样候选](../acceptance/nr/material-mirrored-uv-candidate.md)保留 Intel 阴影差异。
公开材质绑定通过所属检查，相机投影与真实深度来源继续推进。

[材质主 UV 绑定](../acceptance/nr/material-primary-uv-policy.md)通过三十六组实际 GPU 检查。
两配置的 855 份程序逐字节一致，原始四帧摘要精确一致。
公开覆盖、阴影及足迹正控制通过，实际相机与网格组合继续验收。

[实际网格位置](../acceptance/nr/actual-world-depth-source.md)通过完整区域的来源核对。
两设备的可见量均与原生位置控制逐字节一致。
Intel 九十六处深度超差继续修正，场景级相机契约继续接入。

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

[参考坐标帧与世界坐标深度](../acceptance/nr/reference-position-frame.md)通过双配置、双设备核对。
公开世界坐标路径的完整深度平面与 CPU 预转换控制逐位一致，原始四帧摘要精确一致。
实际索引、朝向及 UV 组合继续独立验收。

[实际索引与正面方向](../acceptance/nr/actual-geometry-index-source.md)通过双设备来源核对。
两份完整深度平面与来源控制逐位一致，实际 UV 组合继续验收。

[实际网格主坐标来源](../acceptance/nr/actual-geometry-uv-source.md)完成双设备的实际位置、索引及 UV 组合核对。
完整深度平面及可见量与来源控制一致，后续接入统一场景视图。

[参考相机视图契约](../acceptance/nr/reference-camera-view.md)通过两配置空间查询及双设备深度适配。
17017 条来源射线的往返误差满足八位子像素门槛，宿主视图选择与绘制绑定继续接入。

[参考光栅位置](../acceptance/nr/reference-raster-projection.md)通过两配置、双设备数值核对。
组合保留设备最终深度及屏幕偏移，场景宿主绑定继续接入。

[项目参考视图配置](../acceptance/nr/project-reference-views.md)通过版本、参数位值、保存重开及目录移动核对。
两配置项目契约及当前原四帧通过，场景选择继续接入。

[场景参考相机选择](../acceptance/nr/scene-reference-camera-selection.md)通过持久锚点、刚性移动及优先级核对。
两配置十二组检查及当前原四帧通过，光栅与拾取消费继续接入。

[保守剔除参数](../acceptance/nr/conservative-frustum-transport.md)通过两配置、两设备及旧模块兼容检查。
128 字节布局及原四帧通过，参考混合平面继续接入。

[参考光栅混合视锥](../acceptance/nr/reference-raster-frustum.md)完成两配置、两设备检查。
有界参数、退化回退及原四帧通过，光栅与拾取继续接入。

[参考相机生产接入](../acceptance/nr/reference-camera-render-binding.md)完成共享标定、光栅、拾取及编辑入口检查。
两配置、两设备的实际几何掩码和原四帧通过，完整外观按所属门槛继续验收。

[原生头发参考光栅](../acceptance/nr/native-hair-reference-raster.md)记录实际生产候选。
NVIDIA 组合保留 43 处超差，Intel 组合保留 597 处。
Intel 深度可见量保留 96 处超差，循环索引保持原输出。
生产覆盖的透明边界及实际可见量消费继续验收。

[原生头发覆盖层级](../acceptance/nr/native-hair-coverage-lod.md)记录独立图源及椭圆层级对照。
Intel 既有峰值点从 166 降至 2 字节，整区峰值为 97。
公开覆盖策略见[椭圆选级](../acceptance/nr/material-ellipse-coverage-policy.md)。
参考深度、实际可见量及完整颜色按所属条件验收。

[原生深度设备对照](../acceptance/nr/native-d3d11-depth-devices.md)核对 D3D11 写入及实际消费。
Intel 的跨接口深度有 17014 个点逐位一致。

[场景参考深度](../acceptance/nr/scene-reference-depth.md)记录实际 Player 的双附件读回。
NVIDIA 冻结深度逐位一致，Intel 保留设备间深度差异。
真实法线、对象可见量及完整颜色继续验收。

[场景参考法线与对象身份](../acceptance/nr/scene-reference-normal.md)接入实际场景输入。
NVIDIA 法线来源控制的完整头发区域满足一字节门槛。
场景图集装配和完整颜色继续按所属条件验收。

[场景参考对象可见量](../acceptance/nr/scene-reference-object-visibility.md)接入当前场景。
图集、逆投影及对象记录的独立控制均完成。
实际输入与材质消费按原门槛继续验收。

[对象可见量逆投影标定](../acceptance/nr/scene-visibility-reference-calibration.md)使用原生片元常量。
NVIDIA 头发区域有 277 处超差，Intel 有 428 处。
对象图集、记录及头发外的参考输入继续按来源验收。

[原生图集与接收输入来源](../acceptance/nr/native-atlas-and-receiver-producers.md)核对双方图集绘制。
两份服饰对应 1,604 处同配对图集超差。
原生头发的图集投影按实际乘加次序逐位重建。

[对象可见量深度方向](../acceptance/nr/visibility-projection-depth.md)分别定义图集与记录方向。
当前场景的原生记录标定继续使用实际图集和参考附件。

[原生对象记录标定](../acceptance/nr/native-object-record-calibration.md)使用原生投影和灯向。
NVIDIA 头发区域与图集单项对照在一字节内一致。
当前图集和完整颜色继续按原门槛验收。

[反射法线存储域](../acceptance/nr/evidence/2026-11-21/reflection-normal-storage-domain/README.md)核对量化范围和实际材质绑定。
两区方向差异继续核对几何基底与材质法线的贡献。

[反射几何法线候选](../acceptance/nr/evidence/2026-11-21/reflection-geometric-normal-candidate/README.md)完成控制、成片及恢复核验。
中央地面 SSIM 为 0.83670，环带为 0.75535。
两区与邻区门槛未通过，匹配输入及冻结环境继续待验。

[检修站反射法线对照](../acceptance/nr/evidence/2026-11-21/original-normal-contract/README.md)通过冻结输入与实际绑定核对。
几何法线保留右侧 97 个命中，历史接收仍为零。
统一几何法线候选淘汰，匹配输入及冻结环境继续待验。
