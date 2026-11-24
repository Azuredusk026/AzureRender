# 三纹理采样与视口选择集成

日期为 2026-11-12。独立集成工作树汇合 `0909e26`、视口选择修复 `a38475b` 和三纹理采样 `48847a4`。
受检源码树为 `e760340b406697ef3655b81ed5fa6191966acbb4`。

## 材质与视口

投射头发按材质规则为基础颜色、法线和着色参数选择采样层级。视口根据项目选择状态处理空白点选和 Ctrl 点选。
[三纹理采样规则](projected-hair-point-mip.md)和[视口选择验收](editor-empty-viewport-selection.md)记录各自的行为。

Release 材质结构为 3928 字节，头发参数和预备像素的偏移分别为 2824 和 3720 字节。
Debug 材质结构为 4504 字节，两个偏移分别为 3344 和 4296 字节。
两种配置各核对 896 份着色器。三纹理采样涉及 32 个程序，其中 16 个与生产候选逐字节一致，另外 16 个仅在参考可见量和对象 ID 的描述符绑定上有差异。独立集成使用绑定 50 和 51。

## 回归结果

| 配置 | CPU 用例 | 每块显卡的 GPU 用例 | 视口选择 | 可玩编辑流程 |
| --- | ---: | ---: | ---: | ---: |
| Debug | 18 | 7 | 70.13 秒 | 136.47 秒 |
| Release | 18 | 6 | 59.24 秒 | 16.52 秒 |

两种配置在 NVIDIA 和 Intel 上的五类参考附件及三帧画面与生产候选逐字节一致。
Release 在 NVIDIA 上核对固定和索引描述符两条路径，附件和画面逐字节一致。
两种配置的原始四帧均匹配冻结摘要，验证层和卸载检查通过。
Debug 可玩编辑流程的归档包含一次 240 秒超时和同一门槛下的通过记录。

完整参考接收超差为 NVIDIA 237 处和 Intel 494 处。头发 RGB 超差为 NVIDIA 0 处和 Intel 32 处。
整体画质状态为 `failed-user-review`。

[结果清单](evidence/2026-11-12/projected-hair-viewport-integration/results.json)和
[验证归档](evidence/2026-11-12/projected-hair-viewport-integration/validation.zip)保存验收依据。

## 主线注册用例

投影头发和表面响应的注册命令均接收 Player 路径。Debug 命令同时传入 `--require-validation`。
两种配置分别通过两项实际 GPU 用例和四项构建契约检查。
核对了 Release 1294 份和 Debug 1287 份产物的摘要，原有通过结果继续按产物和命令登记。

[结果清单](evidence/2026-11-13/projected-hair-registration/results.json)和
[验证归档](evidence/2026-11-13/projected-hair-registration/validation.zip)保存生成命令、产物摘要及测试记录。
