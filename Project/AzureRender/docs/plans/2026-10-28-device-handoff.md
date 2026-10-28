# 原生渲染开发交接

日期：2026-10-28。工作分支为 `engine-dev`。
本页记录另一台设备恢复开发所需的状态和资源。
开发规则见仓库根目录 `AGENT.md`。
源码定位见 [源码导航](../development/source-navigation.md)。
完整阶段目标见 [原生还原交接](2026-10-22-native-render-restoration-handoff.md)。

## 当前状态

NR0 为 Active，NR1 至 NR12 为 Pending。
完整画面 SSIM 0.95 和持续 1080p60 仍待验收。
最近一次完整场景 GPU 短测平均 25.008 毫秒。
该结果尚未达到 16.67 毫秒预算。

公共采样器和反射空间过滤已有独立验收。
见[采样器证据](../acceptance/nr/evidence/2026-10-28/gltf-sampler-contract.json)及[反射过滤验收](../acceptance/nr/reflection-spatial-filter.md)。
截图记录见[阶段画面](../acceptance/nr/visual-history.md)。
反射中间纹理用于诊断，其模糊画面不能作为最终场景。

## 当前反射工作

反射颜色与可信度层级资源位于 `src/render/ReflectionSignalPyramid.*`。
其计算程序为 `shaders/reflection_signal_pyramid.comp`。
真实帧基础载荷和逐级参考位于[层级夹具](../acceptance/nr/evidence/2026-10-28/reflection-pyramid-fixture/fixture.json)。
原版重建的六级颜色和可信度均逐字节一致。
Debug 和 Release 的输出均逐级匹配十二份数据。

公共资源已进入主场景计算生产装配。
组合契约见[屏幕反射](../runtime/screen-reflection.md)。
双配置相关分项测试通过，完整场景继续验收。
可信度消费已通过双配置检查。
主场景已接入四通道表面快照，完整图片保持逐像素一致。
公共计算射线和深度层级已通过双配置分项检查。
主场景对照及短测见[接入验收](../acceptance/nr/screen-reflection.md)。
主场景使用半分辨率生产，完整命中输入保持原始尺寸。
来源及数值检查见[尺寸验收](../acceptance/nr/reflection-resolution.md)。
后续核对原版层级射线及时间历史等价。
历史颜色和可信度的公共计算分项已通过双配置检查。
原版重建及可迁移夹具见[时间过滤验收](../acceptance/nr/reflection-temporal.md)。
主宿主共享运动附件已通过双配置资源检查。
默认拥有对照的 80 帧 PNG 与候选逐字节一致。
数值及画质待项见[共享验收](../acceptance/nr/shared-motion.md)。
逐视图反射历史及其生产顺序已进入主场景。
公共组合已支持射线、时间核、层级与空间过滤的连接。
预算及连续槽位见[时间组合验证](../acceptance/nr/reflection-temporal-composition.md)。
主场景通过前置运动和深度拒绝接收历史。
公共运动的重投影和深度拒绝通过双配置检查。
接收规则及来源边界见[重投影验证](../acceptance/nr/reflection-reprojection.md)。
实际引擎截帧确认地面像素接收历史。
双配置、取消后重试及完整画面见[装配验收](../acceptance/nr/main-reflection-history.md)。
当前运行目录为 `build/runs/20261029-025624-main-reflection-history-3693f3`。
该目录的可重建导入缓存按任务结束清理。
原始截帧和完整候选仍由本机保留。
逐事件计数已定位不透明快照的高耗时绘制。
普通不透明快照已复用公共材质特化。
十二帧完整画面及 144 张夹具 PNG 与对照逐字节一致。
实现、双配置及性能见[快照预算](../acceptance/nr/snapshot-shading-budget.md)。
最近场景运行目录为 `build/runs/20261029-045743-snapshot-shading-budget-8ee0b1`。
该目录的导入缓存和临时对照程序在验收后清理。
原版射线的分类、搜索及足迹输入已核对。
重建可信度逐字节一致，颜色与足迹精度仍待验收。
来源及可恢复载荷见[射线核验](../acceptance/nr/native-ray-source.md)。
该内核的主场景接入保持待项。

原始分析位于 `build/runs/20261029-052336-native-hierarchical-reflection-52b603`。
压缩输出及稀疏差异已进入 Git。
重放原版和重建内核时，仍需本机原始 RDC。
角色的服饰及薄透明抗锯齿按既有门槛继续实施。
隐藏平台的运动列表已与颜色可见性同步。
双配置及实际附件见[可见性验收](../acceptance/nr/motion-visible-surfaces.md)。
服饰和薄透明画质保持失败状态，继续处理轮廓与细节。
当前实验目录为 `build/runs/20261029-060530-temporal-cloth-edge-74920a`。
事件与程序指纹见[GPU 热点](../acceptance/nr/reflection-pass-profile.md)。
诊断捕获位于 `build/runs/20261029-044333-reflection-pass-profile-087370`。
该捕获约 1.48 GiB，缓存清理后保留原始证据。
KK 动态夹具通过 NR7-AA5-E1 的双配置边缘门槛。
服饰、薄透明及正式场景继续联合验收。
数值及剩余范围见[运动边缘验证](../acceptance/nr/temporal-motion-edges.md)。
最终画面须使用真实遮罩与探针合成进行对照。

## 环境与资源迁移

仓库保存源码、测试、文档、阶段截图和上述小型载荷。
`build/` 被 Git 忽略，其中的本机运行产物不会随克隆迁移。
原版 3534 帧的 RDC 文件约 945 MB，位于：

```text
Project/AzureRender/build/runs/20261022-201845-endfield-capture-954719/Endfield-games-child_frame3534.rdc
```

完整场景候选和其他原始截帧也保存在本机 `build/runs/`。
需要对这些数据执行原始重放时，应从原设备另行复制对应目录。
原版资源与重放程序的路径由本机环境提供。
另一台设备需安装 Vulkan SDK、Visual Studio C++ 工具链及项目依赖。
RenderDoc 1.46 用于原版帧分析。

## 恢复顺序

1. 克隆仓库并切换 `engine-dev`，核对 `git status`。
2. 阅读本页、开发总计划和反射接入计划。
3. 按本机工具链配置构建 Debug 和 Release。
4. 执行反射层级、空间过滤及 RHI 测试。
5. 对照真实载荷核对层级射线、时间接收和场景性能。
6. 继续角色材质、眼睛、描边、抗锯齿及 NR0–NR12 验收。

多 GPU 程序应串行运行，避免重放与验收相互干扰。
阶段完成后记录测试、画面、性能及资源释放结果。
