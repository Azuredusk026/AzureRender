# 屏幕可见量相位原点

## 公共配置

`ScreenObjectVisibilityView::phaseOrigin` 配置过滤相位的垂直原点。
`TopLeft` 沿图像行号，`BottomLeft` 采用 `height-1-y`。
默认值为 `TopLeft`。
纹理地址、投影逆矩阵的垂直约定及像素偏移分别保持各自的含义。
调用方根据资源来源选择相位原点。

配置由 RenderCore 打包，片段程序仅在选择四乘四旋转相位时使用。
参数占用现有控制字，逐槽参数仍为 992 字节。
输出图像、描述符、逻辑预算及生命周期沿用[公共通道](screen-object-visibility.md)。
非法枚举在参数上传及命令录制前拒绝。
原点配置的适用依据见[设备深度输入](receiver-device-depth.md)。

## 功能检查

用例采用不均匀的十六位图集及固定投影。
实际输出具有可观察的相位差异。
顶部和底部原点的八位附件按垂直翻转关系逐字节一致。
输入纹理保持原地址，深度重建参数在两组中相同。

GPU 用例先保存缺失相位行为的失败记录。
生产实现的 Debug、Release 主程序及公共模块构建通过。
两配置各一项契约及 NVIDIA、Intel 各一项 GPU 检查，共六项通过。
每设备每配置包含八组、共 1672 个实际读取像素。
覆盖原六组输入和新增两组相位原点。
同步验证错误为零，释放后缓冲和图像均为零。

## 默认原生区域

校准程序按当前公共接口重新编译，原始图集、法线及深度按既有路径读取。
默认相位的完整遮罩沿用原 17017 个位置。

| 设备 | 逐字节一致 | 一字节以内 | 最大误差 |
| --- | ---: | ---: | ---: |
| NVIDIA GeForce RTX 4060 Laptop GPU | 17017 | 17017 | 0 |
| Intel RaptorLake-S Mobile Graphics Controller | 16719 | 16842 | 33 |

NVIDIA 原生区域通过默认输出核对。
Intel 的原生区域有 175 处超出一字节，该精度项保持未通过。
本单元的原点配置功能和原生精度分别判断。
主场景集成、完整头发颜色及整体画质继续待验收。
原 CPU 八毫秒及 GPU 16.6 毫秒预算持续有效。

[实施计划](../../plans/2026-11-08-visibility-phase-origin.md)、
[结果清单](evidence/2026-11-08/visibility-phase-origin/results.json)
及[精简证据](evidence/2026-11-08/visibility-phase-origin/validation.zip)
保存缺失行为、实际设备结果、来源和命令。
