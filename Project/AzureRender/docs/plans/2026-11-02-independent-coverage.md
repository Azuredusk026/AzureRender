# 独立裁剪图实施计划

> 执行按 `AGENT.md` 的自主实施规则逐项推进。
> 各项依次完成失败控制、实现、复测和提交。

目标：按裁剪图自身的尺寸、层级和采样器计算覆盖。
归属：Q1 头发边缘、深度和阴影。
状态：公共覆盖与错误退出通过，原生运动及生成层级检查执行中。

架构由资产模块解释独立裁剪图。
渲染模块共享颜色、快照及阴影的覆盖语义。
裁剪图可复用空闲的金属粗糙度纹理槽。
该所有权与物理参数图严格互斥。

技术栈沿用 C++17、Vulkan、GLSL 及 Python。
来源规格见[头发深度覆盖](../acceptance/nr/hair-depth-coverage.md)。
最终交付与画质门槛沿用[开发总计划](azure-engine-plan.md)。

## 来源与边界

原生裁剪图为 512×512，具有十个层级。
基础颜色为 2048×2048，具有十二个层级。
两图采用线性过滤、最近层级及负一层级偏移。
片元使用红通道及 0.5 阈值进行裁剪。

放大后的前两级存在实际采样差异。
排除纹理边界后，中心网格分别出现 57 和 61 次裁剪变化。
所测最大透明度误差分别约为 0.210 和 0.396。
基础图第二级与原生裁剪图零级保持精确一致。

该结果属于实际图像上的标量双线性采样。
像素足迹、硬件采样和最终图像分别验证。
源图、摘要及测量保留于以下目录。

```text
build/runs/20261102-211519-native-hair-coverage-293d4c/
```

公共能力先支持通用、织物和头发的裁剪表面。
输入通过版本二画像的可选 `coverageTexture` 表达。
纹理通道支持红色及透明度，缺省为红色。
加载器按原尺寸逐级写入内部透明通道。

完整材质文档及图保存共享该输入。
裁剪图拥有独立的图像尺寸、层级和采样器。
主颜色、深度、快照、对象阴影及级联阴影消费同一覆盖。
基础颜色及其透明通道保持所属颜色计算的语义。

## 全局约束

- 公共代码使用材质与图像契约表达输入。
- 实际资源身份和纹理名称由内容及诊断工具持有。
- 二十二行、352 字节的画像布局保持明确的版本记录。
- 既有九个纹理槽沿用互斥所有权。
- 裁剪图与金属粗糙度图、相关镜面输入须拒绝冲突。
- 缺省配置的三百零九份程序保持字节一致。
- 显式覆盖策略使用具名程序变体。
- GPU 数值、既有像素及原生区域门槛保持有效。
- 四份无关用户文件按原始摘要保留。

## 审查重点

- 非有限值、非法通道、未知字段及版本一显式输入须被拒绝。
- 缺少图像、错误索引及不完整层级应返回来源明确的诊断。
- 物理参数图及镜面透明通道输入与裁剪槽所有权须一致。
- 固定与索引描述符须使用裁剪图自身的采样设置。
- 颜色、深度及阴影须在阈值附近保持相同覆盖。

## 文件职责

| 文件 | 职责 |
| --- | --- |
| `src/assets/GltfLoader.hpp` | 有类型的裁剪输入及图像所有权 |
| `src/assets/MaterialCoverage.hpp` | 通道、版本和组合校验 |
| `src/assets/GltfLoader.cpp` | 严格解析、通道转换及完整层级 |
| `src/assets/MaterialData.cpp` | 完整文档的纹理语义映射 |
| `src/assets/MaterialSurfaceProfile.hpp` | 直接构造输入的组合校验 |
| `src/render/MaterialPipelinePolicy.hpp` | 有限程序策略及后缀选择 |
| `src/render/RenderContext.hpp` | 版本化能力查询 |
| `src/scenes/CharacterMaterialSampling.inc` | 程序创建、缓存及回收 |
| `src/scenes/CharacterSceneRenderer.cpp` | 主颜色、快照、阴影、上传及采集清单 |
| `src/scenes/CharacterMotionPass.inc` | 向运动通道提供同一覆盖图像 |
| `shaders/mesh.frag` | 独立覆盖及物理参数缺省值 |
| `shaders/character_depth.frag` | 深度覆盖与多重采样路径 |
| `shaders/shadow.frag` | 对象与级联阴影的裁剪输入 |
| `shaders/outline_surface_fragment.glsl` | 扩张深度与描边的相同覆盖 |
| `shaders/outline_mask.frag` | 实例轮廓遮罩的覆盖 |
| `src/scenes/CharacterOutlinePass.inc` | 逐材质选择覆盖程序 |
| `CMakeLists.txt`、材质数据模式 | 构建、公共检查及文件契约 |

修改前按源码导航确认阴影及纹理映射的实际入口。
测试按所属资产、策略和 GPU 工具目录分别维护。
渲染器能力通过接口版本及支持字段声明。
新增程序与缺省程序的来源分别冻结。

## P0 来源与失败控制

- [x] 核对实际图像、层级、采样器和既有未通过范围。
- [x] 对实际图像建立标量采样差异及精确层级控制。
- [x] 建立同机位的原尺寸和放大覆盖 GPU 对照。
- [x] 保存硬件输出、输入摘要及阈值附近的可见变化。
- [x] 建立新字段的解析、非法组合及完整文档失败测试。
- [x] 运行对应资产目标，确认有效失败及缺省兼容。

GPU 对照采用无光照白色裁剪平面及黑色背景。
两组只改变覆盖输入的尺寸与对应层级。
实际原生裁剪图和合成边界分别进入检查。
本项为公共机制和原生输入提供独立依据。

两条描述符路径的原尺寸图均与源掩码逐像素一致。
放大覆盖均产生 62 个不同的裁剪像素。
固定与索引路径的输出分别逐像素一致。
该结果限定为同机位的实际覆盖平面。

## 接口与检查入口

`AssetMaterial::CoverageTexture` 持有存在标识和通道枚举。
`coverageTexture` 接收版本、纹理索引及通道。
`channel` 支持 `red`、`alpha`，缺省为 `red`。
选定通道逐级复制到内部图像的透明通道。

显式裁剪需要版本二画像及 `MASK` 表面。
允许通用、织物和头发类型。
金属粗糙度图及其镜面、AO 消费与裁剪所有权互斥。
已有图层表面的专用纹理所有权分别校验。

材质特性位二十五声明独立覆盖。
裁剪图透明度持有覆盖，RGB 保存常量物理参数。
各层级的粗糙度和金属度沿用纹理缺省时的换算。
深度及阴影的显式变体从同一纹理槽读取覆盖。

运动通道通过既有图像输入消费所选透明通道。

渲染器 API 为九，声明 `supportsMaterialCoverage`。
采集清单记录材质、通道及裁剪图实际尺寸。
画像布局保持二十一，九个纹理槽保持现行数量。
有类型构造和文件解释共享合法组合校验。

新增资产检查位于 `tests/MaterialCoverageTests.cpp`。
目标名为 `AzureMaterialCoverageTests`。
CTest 名为 `AzureRender.MaterialCoverage`。
公共图像检查位于 `tools/test_material_coverage_gpu.py`。

先运行下列主配置检查，再执行所属 GPU 检查。
新字段须先在冻结实现上形成可定位的失败。

```powershell
cmake --build build/ninja-msvc-release --target AzureMaterialCoverageTests AzureMaterialPipelinePolicyTests AzureRenderExtensionRegistryTests AzurePlayer AzureMaterialValidator --parallel 4
ctest --test-dir build/ninja-msvc-release -R "AzureRender.(MaterialCoverage|MaterialPipelinePolicy|ExtensionRegistry)$" --output-on-failure
python tools/test_material_coverage_gpu.py --executable build/ninja-msvc-release/AzurePlayer.exe --expected-validation disabled
```

## P1 公共输入与覆盖消费

- [x] 实现有类型字段、严格版本及纹理所有权检查。
- [x] 保留原尺寸和完整层级，逐级转换选定通道。
- [x] 实现完整文档、参数覆盖和图保存的往返。
- [x] 为显式策略构建颜色、快照、深度及阴影程序。
- [x] 统一样点及像素积分的覆盖采样。
- [x] 核对新增程序的失败回收及正常卸载。
- [x] 通过资产、文档、材质策略及渲染接口检查。
- [x] 核对生成层级的覆盖保持及颜色透明度归属。
- [x] 同步运行契约、数据模式和构建记录后提交。

生成层级按[专项验收](../acceptance/nr/material-coverage-mips.md)核验。
两配置各十六次 GPU 采集要求逐像素一致。
来源 DDS 层级及原生裁剪图分别执行受影响回归。

通用平面与投影头发分别验证公共复用。
不具有参数图的表面使用原有物理参数缺省值。
缺省策略保持所属图像与着色行为。
新增颜色分支按覆盖条件选择具名程序。

## P2 原生接入与受影响回归

- [x] 通过两种描述符的实际采样及裁剪边界对照。
- [x] 核对深度、对象阴影、级联阴影和四倍多重采样。
- [x] 使用原生裁剪图建立独立角色和完整世界候选。
- [x] 采集近景与全身的静态及时间抗锯齿序列。
- [x] 核对既有原生控制点及完整头发的一字节门禁。
- [ ] 保留每项失败结果并继续所属原因定位。
- [x] 执行两配置构建、受影响回归及默认严格像素检查。
- [x] 同步验收证据、总计划、说明及保留清单后提交。

头发颜色、KK 高光、皮肤和场景 GI 分别执行所属验收。
覆盖机制通过仅证明其声明的输入及消费范围。
完整画质和持续性能按既有交付条件验收。

四百帧采集及同条件对照见[联合验证](../acceptance/nr/native-coverage-integration.md)。
匹配输入的全身与近景区域通过所属静态检查。
完整头发的一字节门禁仍未通过。
原冻结服饰参考及整体画质保留失败状态。
