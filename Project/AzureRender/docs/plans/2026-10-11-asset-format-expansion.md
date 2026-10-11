# 资产输入与输出格式拓展实施计划

> 文档类型：后续实施计划
> 状态：G20 自动门禁通过，G21 执行中
> 日期：2026-10-11
> 范围：模型、图像、音频及资产交换工作流

## 阶段推进与人工复核

P5 剩余归因、正式性能及最终交付在 P8 验收。
中间阶段按自身功能、回归和发布门禁推进。
原预算、失败证据及最终来源要求保持有效。
详见[P8 集中门禁](2026-10-12-p5-deferred-to-p8.md)。


各阶段人工复核统一在 P8 执行。
阶段推进依据所属阶段的非延期自动门禁。
人工结果保持待验，最终交付需集中复核通过。
范围与状态见[自动执行计划](2026-10-11-automatic-through-p8.md)。


## 目标与执行依据

用户可以导入常见资产，编辑并用于独立项目。
用户可以按用途导出资产，检查转换损失。
导入、重导入和发布共用资源与任务契约。
公共能力同时服务探索与机械检视项目。

先完成 U15、U16 和 P6 的非延期自动门禁。
随后按本计划连续实施并逐阶段提交。
阶段之间直接推进，完成状态由验收证据确定。
需要操作者完成的门禁按实际执行记录。

依据见仓库根目录的 `AGENT.md`。
当前任务见[P5 收口计划](2026-10-10-p5-closure-and-user-experience.md)。
执行状态见[开发总计划](azure-engine-plan.md)。
机器状态见[格式阶段清单](asset-format-manifest.json)。

当前验收待项与格式阶段依赖见[后续开发路线](2026-10-11-next-development.md)。

技术基础沿用 C++17、CMake、vcpkg 和 Python。
模型运行格式沿用 glTF 2.0 与 GLB。
图像处理复用 stb 与 OpenEXR。
音频运行复用 miniaudio 与离线混音入口。

## 已核验的能力边界

本表来自源码检索，表示当前实现范围。
第三方库的解码能力需通过独立夹具确认。
完整支持须同时满足登记、导入、使用与发布。
导出声明需单独给出格式和保真范围。

| 分类 | 当前入口与边界 | 源码依据 |
| --- | --- | --- |
| 模型导入 | 接受七种模型输入，后台准备与校验 | `src/editor/AssetImportJob.cpp` |
| 模型运行 | 加载网格、材质、骨架及动画 | `src/assets/GltfLoader.cpp` |
| 图像登记 | 九种第一批输入格式 | `src/runtime/AssetTypeRegistry.hpp` |
| 材质图像 | 统一精度解码，按槽转换为八位像素 | `src/assets/GltfLoader.cpp` |
| 环境图像 | 统一解码，保持线性半精度范围 | `src/render/EnvironmentAsset.cpp` |
| 图像重导入 | 解码、设置校验与语义化 mip 生成 | `src/editor/AssetImportJob.cpp` |
| 音频登记 | WAV、MP3、FLAC、Ogg Vorbis | `src/runtime/AssetTypeRegistry.hpp` |
| 音频运行 | 统一探测、预解码或流式播放及离线混音 | `src/runtime/AudioRuntime.cpp` |
| 程序化模型 | 专用 glTF 写入器 | `src/assets/generators/ProceduralGltfWriter.cpp` |
| 内容修改 | 事务、引用维护、撤销与恢复 | `src/editor/content/ContentOperationService.cpp` |
| 独立发布 | 依赖收集、目录移动与清单校验 | `tools/build_game.py` |

`IAssetImporter` 当前只声明 `supports(path)`。
格式执行服务需要独立声明输入与输出契约。
资产类型分类与编解码能力各有明确职责。
现有写入器需经过通用导出测试才可复用。

## 格式目标与优先级

第一批覆盖常见制作资产与开放交换格式。
第二批覆盖压缩纹理和压缩音频交换需求。
每个表格单元表示计划交付能力。
实现状态由阶段清单和夹具结果维护。

| 分类 | 第一批输入 | 第一批输出 | 能力范围 |
| --- | --- | --- | --- |
| 模型 | glTF、GLB、FBX、OBJ、STL、PLY、DAE | glTF、GLB、OBJ、STL、PLY | glTF/GLB 保留受支持场景数据；其他输出按静态几何范围 |
| 图像 | PNG、JPEG、BMP、TGA、PPM、HDR、EXR、TIFF、WebP | PNG、JPEG、BMP、TGA、HDR、EXR、TIFF、WebP | 整数贴图、线性浮点图像与显式转换 |
| 音频 | WAV、MP3、FLAC、Ogg Vorbis | WAV、FLAC、Ogg Vorbis | 解码、声道与采样率转换、明确有损编码 |

| 分类 | 第二批输入 | 第二批输出 | 能力范围 |
| --- | --- | --- | --- |
| 纹理 | DDS、KTX2 | DDS、KTX2 | 二维贴图与 mip；压缩能力按设备选择 |
| 音频 | Ogg Opus、AAC/M4A | Ogg Opus、MP3 | 编解码适配、容器识别与发布许可 |

FBX 与 DAE 的导出通过 glTF/GLB 交换完成。
骨架与动画重点覆盖 glTF、GLB 和 FBX 输入。
其他格式按其实际表达能力报告保留字段。
GIF、PSD、SVG、USD 等记录为后续候选。

第一批是 G17 至 G21、U17 和 P7 的验收范围。
第二批是 G22、G23 和 P8 的验收范围。
两批均纳入用户授权的连续执行路线。
编码器可分发性由相应阶段核验并记录。

## 架构与公共契约

类型注册描述编辑用途、放置能力和稳定标识。
格式注册描述容器、内容探测和读写能力。
领域适配器负责模型、图像或音频转换。
任务服务负责进度、取消和结果发布。

格式注册位于 `src/assets/AssetFormatRegistry.hpp/.cpp`。
共享传输位于 `src/assets/AssetTransfer.hpp/.cpp`。
模型、图像、音频适配器置于对应子目录。
UI 和自动入口调用同一资产操作服务。

格式注册与传输接口已通过 G17 自动验证。

```cpp
enum class AssetDomain { Model, Image, Audio };
struct AssetFormatDescriptor;
struct AssetTransferRequest;
struct AssetTransferReport;
class AssetFormatRegistry {
public:
    void add(AssetFormatDescriptor descriptor);
    const AssetFormatDescriptor& require(const std::string& id) const;
    std::vector<AssetFormatDescriptor> list(AssetDomain domain) const;
};
AssetTransferReport prepareAssetTransfer(
    const AssetTransferRequest& request,
    const AssetFormatRegistry& registry,
    const std::function<bool()>& cancelled);
```

描述包含稳定格式标识、扩展名和探测器。
能力字段分别声明读取、写入和支持特性。
重名扩展按探测结果解析，歧义返回诊断。
请求包含操作、格式、来源、目标与设置版本。

报告包含输出、依赖、警告和转换损失。
报告记录版本、输入哈希、设置与生成器。
暂存结果由主线程验证并发布。
过期来源、取消和失败保持现有内容有效。

操作计划为 `asset.formats`、`asset.import`。
导出计划为 `asset.export` 与 `asset.export-plan`。
重导入继续使用 `asset.reimport`。
参数模式进入公共描述与生成契约。

## 全局验收规则

输入源与转换产物分别记录来源和版本。
资产标识在重导入时保持稳定。
转换缓存按输入、依赖、设置与工具版本区分。
引用、材质槽和项目移动具有回归证据。

导出计划展示覆盖范围和具体损失。
有损转换通过明确选项确认。
文件覆盖使用操作计划和事务校验。
失败与取消清理本任务暂存产物。

合法资产验证导入、重开、运行和移动发布。
所有格式至少有可再分发夹具。
夹具包含许可、来源和预期字段。
外部生成工具固定版本并保存调用参数。

Debug 与 Release 均执行受影响测试。
NVIDIA 与 Intel 执行材质和纹理运行验证。
探索与平台完整流程保持通过。
性能标准沿用当前阶段的严格预算。

每阶段更新设计、教程、格式矩阵与变更记录。
对应清单记录实测状态和证据路径。
独立任务验证通过后创建中文 Git 提交。
用户已有修改按基线保留。

## 重点失败输入

| 条件 | 预期结果 | 所属阶段 |
| --- | --- | --- |
| 中文路径、空格、大写扩展名或伪装扩展 | 正确探测，或给出内容不符诊断 | G17 |
| 缺失外部贴图、层级镜像与非均匀缩放 | 依赖诊断；正确几何、法线与动画 | G18 |
| 高精度、透明、颜色空间和超大图像 | 保留语义；超限拒绝与显式转换 | G19 |
| 截断音频、长文件、声道及循环差异 | 有界解码；正确时长、采样与释放 | G20、G23 |
| 取消、磁盘不足、引用过期或目标冲突 | 明确失败与可恢复事务 | G21、U17 |

## G17：格式注册与传输契约

修改 `src/runtime/AssetTypeRegistry.hpp`。
扩展 `src/extensions/IAssetImporter.hpp` 的领域归属。
实现本计划声明的格式注册与传输类型。
扩展 `AssetImportJob` 的准备与发布契约。

- [x] 新建 `tests/AssetFormatRegistryTests.cpp`。
- [x] 先验证探测歧义、非法描述及大小写匹配失败。
- [x] 实现注册、能力查询与版本化请求校验。
- [x] 接入现有 glTF 导入，保持既有行为通过。
- [x] 验证中文路径、取消、过期与暂存恢复。
- [x] 在 CMake 注册 CPU 测试并更新公共契约。
- [x] 同步格式参考与来源清单，提交 `feat(g17)`。

交付为可查询的格式能力与共享执行边界。
验证使用 `AzureAssets.FormatRegistry`。
既有 `AzureEditor.AssetWorkflow` 必须通过。
界面格式列表由注册描述生成。

## G18：模型互操作与动画保真

新建 `src/assets/model/ModelFormatAdapter.hpp/.cpp`。
复用 `GltfLoader` 与资源缓存契约。
评估 Assimp 的导入器及许可，固定依赖版本。
转换输出进入 glTF/GLB 运行管线。

- [x] 新建静态、双材质槽和骨骼动画夹具。
- [x] 先测试 FBX/OBJ/STL/PLY/DAE 的导入失败。
- [x] 实现轴向、单位、三角化与材质依赖映射。
- [x] 实现骨架、绑定矩阵和动画通道映射。
- [x] 验证镜像缩放、切线、透明与材质槽稳定性。
- [x] 对不支持的曲线、约束和着色节点报告损失。
- [x] 重导入后验证实体引用、动画与保存重开。
- [x] 更新模型指南与许可，提交 `feat(g18)`。

验证使用 `AzureAssets.ModelFormats`。
受影响渲染同时运行材质与角色回归。
Blender 生成夹具时记录版本与脚本。
插件式适配保持核心加载器的职责清晰。

## G19：图像解码与精度语义

新建 `src/assets/image/ImageCodec.hpp/.cpp`。
统一材质、预览与环境贴图的图像描述。
复用 `TextureImportSettings` 与 mip 语义。
TIFF/WebP 评估对应库并固定许可与版本。

- [x] 新建 `tests/ImageCodecTests.cpp` 与颜色夹具。
- [x] 先验证格式、精度、透明和错误输入断言。
- [x] 接入第一批全部图像输入格式。
- [x] 描述像素类型、颜色空间、方向与通道。
- [x] 为材质显式转换高精度输入并记录损失。
- [x] 环境图像保持线性浮点范围与亮度。
- [x] 验证灰度、十六位、负窗口与畸形尺寸。
- [x] 验证 mip、采样和双设备实际像素。
- [x] 更新图像说明与许可，提交 `feat(g19)`。

验证使用 `AzureAssets.ImageFormats`。
图像尺寸与解码预算沿用既有受支持上限。
新增上限需有单独预算和压力验证。
预览和运行使用一致的方向与颜色解释。

## G20：音频解码与资源生命周期

新建 `src/assets/audio/AudioCodec.hpp/.cpp`。
修改 `src/runtime/AudioRuntime.cpp` 的资源接入。
配置 WAV、MP3、FLAC 与 Ogg Vorbis 解码能力。
通过离线混音验证样本，再复核真实听感。

- [x] 新建 `tests/AudioCodecTests.cpp`。
- [x] 使用可再分发正弦、脉冲和静音夹具。
- [x] 先验证四格式导入、时长与样本断言。
- [x] 实现采样率、声道、循环和解码策略。
- [x] 明确短音效预解码与长音频流式边界。
- [x] 验证截断、错误容器、取消与重复释放。
- [x] 验证预览、场景切换和独立发布。
- [ ] 在 P8 记录操作者听感与输出设备。
- [x] 更新音频指南与许可，提交 `feat(g20)`。

验证使用 `AzureAssets.AudioFormats`。
离线结果覆盖时长、波形、峰值与静音。
实体听感记录播放、循环和音量体验。
格式支持声明与设备可用状态分别维护。

## G21：通用导出与往返验证

新建 `src/assets/AssetExportJob.hpp/.cpp`。
实现模型、图像和音频第一批输出适配器。
复用内容事务、生成清单与公共任务服务。
将程序化写入器的公共部分按实际需求复用。

- [ ] 新建 `tests/AssetRoundTripTests.cpp`。
- [ ] 先验证格式能力、损失计划与覆盖冲突。
- [ ] 实现选中资产和场景子树的导出计划。
- [ ] GLB/glTF 保留受支持几何、材质和动画。
- [ ] 静态格式报告层级、材质和动画损失。
- [ ] 图像验证精度、颜色、透明与有损容差。
- [ ] 音频验证采样率、声道、时长和编码质量。
- [ ] 验证导出后重导入、项目移动和依赖完整性。
- [ ] 验证取消、磁盘写入失败与目标恢复。
- [ ] 更新交换教程与证据，提交 `feat(g21)`。

验证使用 `AzureAssets.RoundTrip`。
无损数值按浮点容差及离散字段相等验证。
有损编码在夹具中固定误差阈值。
引擎脚本、物理与任务保存在项目格式中。

## U17：导入、导出与重导入体验

修改 `AssetBrowserPanel.cpp` 与 `TextureAssetPanel.cpp`。
修改 `AssetEditorRegistry` 与公共操作注册入口。
通过 `EditorSession` 调度资产任务。
自动工具使用相同参数模式与结果诊断。

- [ ] 新建 `tools/test_asset_transfer_workflows.py`。
- [ ] 先验证文件筛选、能力列表与错误反馈。
- [ ] 实现按注册描述生成的格式与设置界面。
- [ ] 展示来源、转换设置、损失与依赖问题。
- [ ] 验证进度、取消、重试及未保存文档保护。
- [ ] 验证材质槽、引用和稳定资产标识。
- [ ] 两种制作工作流完成真实 UI 回放。
- [ ] 操作者复核路径、反馈与恢复理解性。
- [ ] 更新教程与交互验收，提交 `feat(u17)`。

验证使用 `AzureEditor.AssetTransferGpu`。
界面、菜单和自动入口共享生产操作。
单击选择、双击定位与右键漂浮保持通过。
实体操作与自动回放分别记录覆盖。

## P7：第一批格式的制作与交付验收

- [ ] 两个空项目导入三类资产并完成制作。
- [ ] 完成修改、重导入、导出与往返校验。
- [ ] Debug/Release 执行完整受影响回归。
- [ ] NVIDIA/Intel 验证材质、动画与纹理。
- [ ] 独立包隐藏来源后移动并隔离运行。
- [ ] 完整回归探索接平台及失败恢复流程。
- [ ] 在 P8 按当前严格预算执行正式性能与长跑。
- [ ] 核验许可、来源、包哈希和支持矩阵。
- [ ] 归档自动结果及 P8 人工待项，提交 `feat(p7)`。

交付包含安装包、项目包与完整能力矩阵。
每种格式绑定夹具、测试与具体保真范围。
未验证功能保持待验状态。
第一批非延期自动门禁通过后进入 G22。

## G22：压缩纹理与 mip 交换

评估 DirectXTex 与 KTX-Software 的适配边界。
新增 `src/assets/image/TextureContainerAdapter.hpp/.cpp`。
设备支持由 Vulkan 能力查询选择。
不支持的压缩格式使用有预算的转换路径。

- [ ] 先测试 DDS/KTX2 容器、mip 与损坏输入。
- [ ] 实现二维纹理读写和 mip 链保留。
- [ ] 处理 BC 与 Basis 转码的实际设备支持。
- [ ] 验证 sRGB、法线、尺寸和压缩误差。
- [ ] 验证双设备运行、重导入和移动发布。
- [ ] 更新矩阵、工具许可与性能，提交 `feat(g22)`。

验证使用 `AzureAssets.TextureContainers`。
立方体、数组和体纹理单独标注范围。
其可用状态以运行时及夹具证据为准。

## G23：压缩音频与容器交换

新增 Ogg Opus、AAC/M4A 输入适配器。
新增 Ogg Opus 与 MP3 输出适配器。
评估 libopus、LAME 与可分发 AAC 解码方案。
外部转换器采用固定参数与受控进程调用。

- [ ] 先测试容器探测、采样延迟与时长。
- [ ] 核验依赖许可、二进制分发和来源版本。
- [ ] 实现解码、编码、取消与错误定位。
- [ ] 验证标签、声道、循环及有损误差。
- [ ] 验证长文件、异常退出与任务恢复。
- [ ] 验证隔离发布与实体听感。
- [ ] 更新格式说明与证据，提交 `feat(g23)`。

验证使用 `AzureAssets.CompressedAudioFormats`。
转换产物记录编码器参数、延迟与填充。
依赖出现分发硬阻塞时记录具体受限能力。

## P8：完整格式能力收口

- [ ] 复核两批支持矩阵与全部格式夹具。
- [ ] 执行两配置回归、双设备与独立发布。
- [ ] 执行完整游戏流程及严格性能验收。
- [ ] 完成真实资产交换、实体操作与听感。
- [ ] 核验原字节证据、来源清单和包哈希。
- [ ] 同步文档与全部阶段状态，提交 `feat(p8)`。

最终报告列出实际能力、损失范围与剩余限制。
自动阶段完成需要代码、回归与交付证据共同成立。
完整交付同时需要 P8 集中人工复核通过。
