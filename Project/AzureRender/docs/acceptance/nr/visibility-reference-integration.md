# 参考算术主线集成

日期：2026-11-10。
公共实现来源为 `da56ac4`，主线提交为 `1ee5757`。
Debug、Release 宿主及十一项所属目标构建通过。
两配置合计 32 项有效检查通过，跳过数为零。

## 验证范围

公共配置、空后端、渲染图及资源生命周期均通过。
模块边界和依赖夹具分别核对。
默认及参考 GPU 工作流在两配置、两设备均通过。
实际设备为 NVIDIA 独显与 Intel 核显。

Release 首轮实际执行十一项公共检查。
纹理用例的注册名为 `AzureAssets.TextureContainers`。
选择器修正后补跑该项，结果通过。
相同来源的十一项成功结果按范围引用。

## 原始画面与来源

实际 Release 宿主执行原冻结四帧。
四帧的 RGB 差异与 PNG 摘要差异均为零。
1729 项项目输入保持原摘要。
同步日志无警告和错误，卸载分配残留为零。

活动清单包含 678 份程序。
677 份保持原摘要，公共可见量片元程序对应本项实现。
两配置的源码和产物来源核对通过。
主颜色阴影历史及默认隔离实验选项均关闭。

## 后续验收

参考配置及校准范围见[公共契约](visibility-reference-arithmetic.md)。
完整头发颜色和当前场景深度继续独立处理。
CPU 八毫秒、GPU 16.6 毫秒及正式条件保持有效。
整体画质保持 `failed-user-review`。

[结果清单](evidence/2026-11-10/visibility-reference-integration/results.json)
与[精简证据](evidence/2026-11-10/visibility-reference-integration/validation.zip)
保存构建、测试、来源和实际四帧摘要。
