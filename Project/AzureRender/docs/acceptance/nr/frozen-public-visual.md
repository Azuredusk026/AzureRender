# 公开角色冻结画面核验

## 冻结输入

冻结提交为 `eb916a6`。参考目录包含七张 1280×720 图像，严格门槛为 RGB 零差异像素。
生成时保留的七张原始截图与参考 PNG 逐字节一致，显卡为 RTX 4060 Laptop GPU，驱动版本为 NVIDIA 595.79。

## 历史宿主重放

使用同一显卡和 NVIDIA 592.82 驱动构建并重放 `eb916a6`。
默认索引描述符、固定描述符及顶点着色器蒙皮三条路径的像素统计一致。

| 图像 | 差异像素 |
| --- | ---: |
| 成片 | 3 |
| 反照率 | 0 |
| 世界法线 | 0 |
| 材质标识 | 0 |
| 描边 | 0 |
| 直射漫反射 | 6 |
| 面部距离场 | 0 |

成片与直射漫反射的九处差异均为单通道一个色阶。
重放构建与生成证据中的 41 份着色程序摘要全部吻合。
两次的宿主程序摘要不同，生成清单的 340 份源码中有 297 份与受检提交吻合。
同机 595.79 重放结果见冻结驱动复测。

## 主线背景对照

在主线 Release 宿主中替换两份背景着色程序，保留其余着色程序及公开资产。
成片的差异从 495756 像素降至 25239 像素。
反照率和世界法线对照图与主线默认输出逐像素一致。
三条主线注册检查的严格阈值继续有效。

## 主线显示和镜头对照

独立候选使用屏幕空间背景、展台调色及同通道空间抗锯齿。
恢复固定镜头的投影系数计算后，反照率、世界法线、材质标识、描边和面部距离场达到零差异。
成片差 11 个像素，直射漫反射差 15 个像素。
默认入口的七张图与受检主线输出逐像素一致。
[镜头对照归档](evidence/2026-11-13/frozen-public-visual/lens-control.zip)保存命令、图像、着色程序摘要和测试结果。

## 主线级联阴影对照

候选在展台捕获路径采用对应的级联深度范围、遮挡采样和圆盘权重。
默认索引描述符、固定描述符和顶点着色器蒙皮的七张输出均与同机历史宿主的 PNG 逐字节一致。
与冻结原图的差异保留在成片 3 个像素及直射漫反射 6 个像素，均为单通道一个色阶。
本节记录 592.82 的对照结果。
[阴影对照归档](evidence/2026-11-13/frozen-public-visual/shadow-control.zip)保存两种入口的像素结果、构建摘要与候选补丁。

[回退路径归档](evidence/2026-11-13/frozen-public-visual/fallbacks.zip)保存固定描述符和顶点着色器蒙皮的逐图复测。

## Debug 注册对照

隔离 Debug 构建通过命令行测试，并使用 592.82 执行三个严格注册用例。
每条路径的七张输出一致，成片有 3 个像素、直射漫反射有 6 个像素各相差一个色阶，其余五张达到零差异。
[逐图结果](evidence/2026-11-13/frozen-public-visual/debug-59282.json)及[注册对照归档](evidence/2026-11-13/frozen-public-visual/debug-59282.zip)保存 21 张截图、捕获清单、设备报告和注册结果。

## 冻结驱动复测

使用 RTX 4060 Laptop GPU 和 NVIDIA 595.79 复测。
历史宿主与独立候选各执行三条路径，每条路径生成七张图像。
42 张图像全部达到 RGB 零差异。

主线 Debug 与 Release 各通过三个严格注册用例。
每条路径的七张图像均达到 RGB 零差异。
设备报告与捕获清单记录实际描述符路径和蒙皮调度次数。

两种配置各通过 18 项关联回归。
覆盖天空投影、材质显示、抗锯齿、阴影、蒙皮和探索关卡。

展台捕获通过 `--qa-presentation showcase` 启用。
该选项适用于独立角色捕获，支持关闭抗锯齿、边缘抗锯齿与超采样。
捕获清单用 `qaPresentation` 记录显示模式。

[595.79 结果清单](evidence/2026-11-13/frozen-public-visual/driver-59579-results.json)与[验证归档](evidence/2026-11-13/frozen-public-visual/driver-59579-validation.zip)保存逐图摘要、设备报告、捕获清单和注册结果。

2026-11-15 的主线构建复验覆盖同一设备、驱动和三条注册路径。Release 与 Debug 各通过 21 张截图。全部 42 张截图的 RGB 差异为零，PNG 摘要均与冻结文件一致。

[主线复验清单](evidence/2026-11-15/integrated-frozen-public/results.json)和[验证归档](evidence/2026-11-15/integrated-frozen-public/verification.zip)保存两个构建的产物摘要、CTest 结果及逐图检查。

[重放归档](evidence/2026-11-13/frozen-public-visual/replay.zip)保存构建来源、显示设备、严格比对和逐像素证据。
[复验计划](../../plans/2026-11-13-frozen-public-visual-regression.md)记录后续验证条件。

## 着色器注册回归 {#着色器注册回归}

Release 与 Debug 均通过 `AzureRender.ShaderCompilerContract` 和 `AzureRender.ShaderCatalogCompilation`。目录编译逐份核对生成程序摘要，并按注册时限执行。

Release 的首次执行因受管池预留空间不足在编译前退出。任务封存归还未使用配额后，使用同批构建产物及原注册命令复测。[检查收据](evidence/2026-11-14/frozen-public-shader-controls/results.json)与[验证归档](evidence/2026-11-14/frozen-public-shader-controls/verification.zip)保存首次结果、复测和 Debug 对应分组的报告。

历史背景与显示诊断的五组着色器通过内容摘要归档。归档包含 897 份独立内容、4,483 条原路径和逐文件摘要，大小为 89,599,641 字节。冻结图片、正式验收资料与重放程序保留在各自的原地址。

归档与摘要清单登记于[产物清单](../../development/artifact-manifest.json)。在引擎根目录运行下列命令，将文件恢复到新的临时目录，并逐份核对还原摘要：

```powershell
python build/archives/frozen-public-visual-shader-controls-20261114-restore.py --staging build/maintenance/public-shader-recovery/shader-controls
```
