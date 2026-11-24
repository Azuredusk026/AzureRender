# 原生渲染参考基准

日期：2026-10-17。
状态：本机中庭实景已采集，正式标定待完成。

## 清单契约

工具入口为 `tools/native_render/reference_manifest.py`。
契约为 `NativeRenderReference/1`。
工具只读验证摘要、图像、版本和参考状态。
图像编码与尺寸通过实际解码确认。

正式参考的分辨率须与图像像素一致。
损坏或截断图像会导致验证失败。
重复身份和不完整的正式参考会导致验证失败。

正式参考需记录场景、任务、照明及画质状态。
相机记录位置、单位四元数和垂直视场角。
同时记录分辨率、曝光、抗锯齿和采样时间。
引擎画面及网络画面归类为辅助参考。

已采集但尚待标定的游戏画面使用 `observed` 状态。
该状态校验摘要与版本，计入待项。

## 引擎参考输入锁

引擎辅助基准可声明 `reproduction` 子对象。
其版本为一，角色须为 `engine_baseline`。
校验入口和 `NativeRenderReference/1` 返回格式保持一致。
引擎参考继续作为辅助基准登记。

| 字段 | 内容与规则 |
| --- | --- |
| `schemaVersion` | 整数一 |
| `workingDirectory` | 存在的工作目录，路径相对清单解析 |
| `program` | 程序路径、字节数和 SHA-256 |
| `project` | 项目描述文件路径、字节数和 SHA-256 |
| `inputs` | 非空依赖列表，每项具有路径、字节数和 SHA-256 |
| `command` | 原始参数数组，程序和项目须匹配锁定身份 |

文件记录使用 `path`、`bytes` 和 `sha256`。
工作目录和文件路径均以清单目录为基准。
命令内的相对路径按声明的工作目录解析。
输入路径的规范化结果须互不重复。

工具只读核验所列文件及参数身份。
完整依赖范围由对应资产来源清单提供。
着色程序和运行库作为普通依赖记录。
项目语义及参数合法性继续由生产加载器校验。

文件缺失、大小变化及同长度内容变化均拒绝。
命令指向其他程序或项目时同样拒绝。
私有素材保持本机路径，正式记录保存摘要。
程序或输入更新时，候选清单显式记录对应版本。

角色正面参考与完整场景共用该校验入口。
清单见[输入锁记录](../recovery/evidence/2026-10-31/reference-input-lock/reference-manifest.json)。
执行方式如下。

```powershell
python tools/native_render/reference_manifest.py docs/acceptance/recovery/evidence/2026-10-31/reference-input-lock/reference-manifest.json
```

## 本机证据

运行目录为 `build/runs/20261017-151417-native-render-restoration-e7723b`。
其中 `reference-manifest.json` 登记六组待采集视图。
`game-reference/` 保存 13 张带时间戳的中庭画面。
画面可见升降货箱、旋转环带及发光导轨。

用户选定明亮白黄色中庭状态。
已有引擎首帧登记为辅助基线，摘要校验通过。
正式参考数量为零，图像一致性门禁待验收。

执行 `python -m unittest discover -s tools -p test_native_render_reference.py`。
十二项测试覆盖合法参考、缺失状态与错误摘要。
同时覆盖版本混用、辅助分类、重复身份和非法相机。
另验证分辨率、损坏图像及真实编码读取。

测试先确认功能缺失失败，再实现并通过验证。

## 实际画质条件

游戏设置为 2560×1440，全屏，60 帧。
画质为高默认档，阴影及特效为中档。
屏幕空间反射为中档，DLSS 为平衡模式。
帧生成为两倍，色差开启，接触阴影关闭。

锐化为零，光追状态尚未从界面确认。

窗口采集结果为 1707×960。
采集缩放与游戏输出分辨率分别记录。
定时采样用于动态观察，时间间隔以收据为准。
相机和曝光标定完成后再冻结定量比较条件。

## 角色预览与分项诊断

用户已打开莱万汀角色预览。
用户反馈帝江号区域出场角色限定为管理员。
角色预览用于脸部、头发和服装外观参考。
其照明与中庭分别登记，姿态尚待标定。

本轮证据目录为
`build/runs/20261017-175206-nr-playable-integration-d4e789`。
`game-reference/` 保存完整、正面及侧面预览。
这些画面属于已观察参考，定量资格待标定。

公共诊断入口支持保留场景相机和灯光。
参数为 `--qa-camera scene --qa-light scene`。
粗糙度、金属度及着色法线可独立输出。
直接高光和环境高光分别输出，保持原数值尺度。

分项模式关闭背景及轮廓，完整画面保持场景配置。

公共夹具为 `tools/native_render/component_lab.py`。
真实 Player 输出经过线性颜色误差检查。
夹具验证完整画面逐像素一致及相机条件一致。
同时检查运行错误、验证层及 GPU 资源释放。

证据位于 `component-lab-camera-fix/metrics.json`。

隔离项目保留资产身份文件及场景依赖。
`baseline-frames/` 保存实际场景两帧基线。
`scene-beauty-frames/` 的两帧与基线逐像素相等。
六项命令行、动画、物理及运行时短回归通过。

该基线仅确认项目可运行，视觉门禁仍待验收。

## 正式标定待项

采集广场正面、侧面、地面及三个角色近景。
同条件重复采样后，测量噪声并冻结阈值。
相机标定和遮罩固定后进入定量对照。
资源解析及引擎机制实施可独立推进。

## 无遮挡参考与地标标定

用户确认的范围为实景中观测到的全部对象与效果。
本轮拍照模式已隐藏角色和主要界面。
底部仍可见 UID 和网络状态，正式遮罩须排除该区域。
两次后续画面的俯仰不同，分别登记相机。

它们的差异用于观察，固定机位噪声仍待采样。

本轮运行目录为
`build/runs/20261021-125515-native-scene-lighting-5e36f1`。
`game-reference/atrium-photo-front-000.png` 保存接续画面。
该文件的原始采集时间未在接续状态中保留。
`atrium-photo-front-001.png` 与 `002.png` 有采集时间收据。

`photo-reference-manifest.json` 登记文件摘要与待项。
游戏内容版本尚未由当前安装的有效元数据核验。
可执行文件的 Unity 版本分别记录。

公共标定入口为 `native_render/camera_calibration.py`。
输入格式为 `RenderCameraLandmarks/1`。
每个地标记录身份、米制世界坐标和图像像素坐标。
拟合至少需要六个非共面地标。

另需两个独立验证地标，它们不参与拟合。
图像原点位于左上角，纵坐标朝下。

求解采用归一化直接线性变换，仅依赖 NumPy。
输出相机位置、世界轴向、内参和垂直视场角。
每个地标均保存重投影坐标与像素误差。
共面、退化、非有限数据及相机背后的点会被拒绝。

非法身份、分辨率和缺少验证点同样会拒绝。

投影验收检查独立地标及拟合地标的误差上限。
同时检查方形像素、居中主点和近零倾斜项。
透视拟合通过只证明地标投影符合输入预算。
版本、曝光、噪声和画质仍由正式参考门禁验证。

本工具始终输出 `reference_qualified=false`。

执行入口如下：

```powershell
python tools/native_render/camera_calibration.py landmarks.json --image reference.png --output calibration.json
python -m unittest discover -s tools -p 'test_native_render*.py'
```

十四项测试覆盖参考清单与地标标定。
其中五项标定测试覆盖米制位置、旋转及独立验证误差。
错误对应点触发投影失败，共面数据触发拒绝。
证据见 `camera-calibration-final-python.log`。

真实场景地标和最终机位保持待验收。

## 固定机位采集与像素核验

2026-10-21 保存五张无输入间隔的中庭画面。
画面包含上下传送仓、导轨、玻璃与地面反射。
每张收据记录窗口、采集时段、编码与文件摘要。
底部 UID 和网络状态属于遮罩排除范围。

本轮证据根为：
`build/runs/20261021-172156-native-render-evidence-623c52/`。
`game-reference/capture-receipts.json` 保存采集收据。
`game-reference/reference-manifest.json` 通过只读验证。
五张均登记为 `observed`，正式参考数量为零。

UI 采集链返回 1707×960 的 JPEG。
游戏设置记录的 2560×1440 单独保留。
正式比较须明确缩放与压缩产生的影响。
文件扩展名不能独立证明图像编码。

`repeated-frame-observation.json` 保存重复帧测量。
两个观察区域合计 152407 像素。
相对首帧的平均通道绝对差为 2.44% 至 3.21%。
通道中位绝对差均约为 0.392%。

数值采用归一化 sRGB，包含动态照明与采集影响。
这些测量用于调查，正式噪声阈值继续待冻结。

自动 Escape 输入后，拍照界面未出现可见变化。
进程令牌查询返回访问被拒绝。
当前执行进程为非提权状态。
这些结果尚不能确定游戏输入限制的具体原因。

原始画面采集与离线资产工作继续推进。

引擎帧数据链按[证据执行计划](../../plans/2026-10-21-native-render-evidence-execution.md)推进。
公开算法与推测见[渲染资料核查](../../research/2026-10-21-endfield-public-rendering.md)。
