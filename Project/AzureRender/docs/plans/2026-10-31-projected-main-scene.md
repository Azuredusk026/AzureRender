# 主场景投影反射实施计划

执行方式：当前会话自主实施。
目标：通过类型化设置接入当前帧投影反射。
前置提交为 `81678a0`，归属 NR5-R8-R7。
公共组件见[组合计划](2026-10-31-projected-reflection-composition.md)。

## 阶段边界

本阶段将公共组合接入真实场景和 Player。
投影路径消费当前表面及颜色快照。
对象持有的恒定零运动场用于当前帧追踪。
实际采样投影包含所属视图的子像素偏移。

原生投影历史按后续独立任务接入。
该任务负责成功提交、抖动差及运动有效性。
线性反射及其时间过滤继续按现行契约运行。
正式连续序列及持续性能按所属阶段验收。

## 设置与兼容

`ScreenSpaceReflectionAlgorithm` 提供 `Linear` 和 `Projected`。
默认值为 `Linear`，序列化名称为 `linear` 和 `projected`。
反射设置的嵌套版本为二。
版本一按线性算法读取，版本二提供 `algorithm` 字段。

步数、边缘宽度、粗糙度及强度由两类生产共享。
距离、线性厚度和法线偏移属于线性追踪的参数。
全部持久化字段保持各自合法范围和往返语义。
非法算法和版本拒绝先于目标设置变化。

算法选择在视图资源装配时生效。
调用者通过视图重建应用影响资源的设置。
设备或投影条件不满足时记录实际生产和回退原因。
普通参数按逐帧视图快照消费。

## 场景归属

通用准备、射线、深度、信号及过滤归属 RenderCore。
角色场景适配器借用自身的表面和颜色快照。
场景分别持有线性或投影组合对象。
输出通过现有存储图像绑定进入统一材质合成。

资源图和直接记录共用 `recordScreenReflection`。
记录前发布源快照的计算采样可见性。
组合负责中间资源同步，场景负责关闭前退休。
清单记录请求算法、实际生产、资源及回退范围。

## 文件职责

| 文件 | 职责 |
| --- | --- |
| `src/render/ScreenSpaceReflection.hpp` | 算法类型、名称、版本及校验 |
| `src/runtime/LevelRenderSettings.hpp` | 设置编码及版本一读取 |
| `src/scenes/CharacterSceneRenderer.hpp` | 组合所有者及统一输出入口 |
| `src/scenes/CharacterScreenReflection.inc` | 算法资源选择及共同记录 |
| `src/scenes/CharacterSceneRenderer.cpp` | 资源图、直接路径及诊断 |
| `tests/ScreenSpaceReflectionTests.cpp` | 默认值、往返及拒绝事务 |
| `tests/TemporalSettingsTests.cpp` | 现行版本及时间设置兼容检查 |
| `tests/ProjectedReflectionSceneTests.cpp` | 直接记录、槽位、回退及关闭 |
| `tools/test_screen_space_reflections_gpu.py` | 共享镜面几何及真实捕获 |
| `tools/test_projected_scene_reflections.py` | 实际 Player 镜面及路径检查 |
| `CMakeLists.txt` | 受影响验证入口 |

## 实施与验证

- [x] 冻结当前 Release Player 及必要运行文件。
- [x] 编写算法接口红测并保存缺失成员编译失败。
- [x] 实施版本二和版本一的明确读取规则。
- [x] 接入资源选择、共同输出和失败回收。
- [x] 接入资源图和直接记录，并保留实际生产诊断。
- [x] 独立镜面按几何、能量和颜色门槛验证。
- [x] 核对固定绑定、共享绑定、关闭及多样点路径。
- [x] 回归默认线性路径并对照冻结 Player 图像。
- [x] 双配置构建、公共组件及实际 Player 验证。
- [x] 核对验证层、释放、指纹和文档状态。
- [x] 创建独立阶段提交并接续投影历史任务。

冻结副本只保存 Player、动态库和着色程序。
该副本用于默认路径的可审阅图像对照。
全部临时内容由当前任务目录持有。
