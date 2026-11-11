# 项目现状

[材质组合管线范围](acceptance/nr/material-viewquad-pipeline-scope.md)通过工作集及短时成本检查。
正式固定功率、长窗口 GPU 帧预算及完整交付继续验收。

[参考相机渲染主线](acceptance/nr/reference-camera-binding-main-integration.md)通过保守剔除、实际光栅及拾取检查。
双配置、双设备与重采原始四帧通过。

[材质组合深度策略](acceptance/nr/material-viewquad-production.md)通过双设备边界检查及严格四帧。
候选采用显式构建选项，正式帧预算及内存继续验收。

[材质纹理梯度主线接入](acceptance/nr/material-uv-main-integration.md)通过双配置及两设备复验。
十八项检查、六组主线设备检查和严格四帧通过，正式整体条件继续验收。

[独立材质视图](acceptance/nr/material-view-determinism.md)的接收计算及重复渲染回归通过。
双配置、双设备与原始四帧完成所属核对。

## 当前验收状态

性能、原生画质与集中人工复核继续待通过。
既有长期目标为 CPU P95 八毫秒及 GPU P95 16.6 毫秒。
严格四帧、SSIM 与关键区域亮度门槛保持有效。

[参考透明层级](acceptance/nr/reference-alpha-ellipse.md)通过公共策略和原四帧检查。
NVIDIA 完整 17017 处一致，Intel 剩余九十六处深度超差继续修正。

[接收范围](acceptance/nr/clustered-receiver-ranges.md)与
[运动绑定复用](acceptance/nr/motion-descriptor-reuse.md)已接入生产。
当前单元的原生短测与正式长采样分别登记。
[静态图形输入](acceptance/nr/compact-vertex-source.md)未达到采用门槛。
其 28 字节候选 CPU P95 约为 6.39 毫秒，GPU 约为 22.68 毫秒。
该数据属于隔离候选，生产已恢复受检基准。

[Intel 零历史兼容](acceptance/nr/shadow-receiver-entry.md)已通过原断言。
[零贡献计算](acceptance/nr/direct-light-zero-contribution.md)未通过局部效率门槛。
原生头发整区、对象接收精度及完整联合外观继续验收。
[对象阴影页边界](acceptance/nr/object-shadow-page-boundary.md)的串扰修复通过四项设备检查。
原始四帧保持零差，完整接收精度继续验收。
目录维护按 D 盘项目技能与每日有界回收执行。
[材质朝向需求](acceptance/nr/material-facing-demand.md)减少八十八次管线创建。
原四帧与六百七十五份主着色器均保持逐字节一致。
加载耗时与每帧正式性能分别验收。
[实际接收坐标](acceptance/nr/native-producer-gpu-intermediates.md)闭合 NVIDIA 区域的输入精度超差。
[固定采样足迹](acceptance/nr/shadow-gather-footprint.md)闭合 Intel 的九十处区域过滤超差。
四项设备检查及原四帧通过，完整接收重建与头发颜色继续验收。

## 已取得的分项证据

命令准备与缓冲同步通过六项所属检查及原生零像素对照。
短时 CPU 均值为 9.02 毫秒，P95 为 10.31 毫秒。
原帧预算、环境及画质继续验收，来源见[同步优化](acceptance/nr/read-scope-barriers.md)。

动态几何准备通过七项所属检查及原生四帧零像素对照。
短时 CPU 均值为 10.76 毫秒，P95 为 11.51 毫秒。
原帧预算、环境及画质继续验收，来源见[动态几何优化](acceptance/nr/dynamic-geometry-preparation.md)。

专用快照及渲染描述复用通过所属检查与原生零像素对照。
短时 CPU 均值为 13.81 毫秒，P95 为 15.28 毫秒。
原帧预算、环境及画质继续验收，来源见[宿主优化](acceptance/nr/host-scene-preparation.md)。

无光照混合主输出通过所属检查及原生四帧严格对照。
短时 GPU 均值改善约 12%，证据见[着色优化](acceptance/nr/unlit-blend-fast-path.md)。
CPU 成本、完整帧预算、供电及整体画质继续验收。

运行时身份查询通过两配置各十九项检查。
联合性能及供电范围见[身份验证](acceptance/nr/runtime-node-identity.md)。
运动向量绑定通过两配置与双设备检查。
实际命令数量和完整帧耗时见[绑定验证](acceptance/nr/motion-bindings.md)。
帧场景快照复用通过两配置各九项检查。
原生四帧一致，性能条件见[快照验证](acceptance/nr/frame-context-snapshot.md)。
相机共享场景输入通过两配置各十四项 CPU 检查。
完整批次及目录规模待项见[共享输入验证](acceptance/nr/render-camera-snapshot.md)。
完整画质与实时性能仍待验收。

原生可玩还原按 NR0 至 NR12 推进。
NR0 至 NR8 为渲染主线，NR9 至 NR11 为角色操控。
NR12 验收近乎原生画面、WASD 和运动动画混合。
任务与引擎扩展见[可玩还原计划](plans/2026-10-17-native-playable-restoration.md)。
参考清单九项测试通过，已采集本机中庭定时画面。
白黄中庭状态已选定，传送仓动态纳入还原范围。
六组正式视图及相机标定仍待完成。
环境入口的 23 包依赖已读取，原生渲染机制正在实施。
聚光与显式场景照明已通过公开 GPU 夹具。
预滤波 HDR 层级通过读取、上传和采样验证。
原生 GI 体积已完成世界查询与方向系数解码。
通用方向网格已接入 GPU，六组像素对照通过。
中庭两套 GI 候选各通过四帧联合采样。
契约见[方向辐照网格](runtime/irradiance-volumes.md)。
独立局部反射支持原作层级、盒投影与体积混合。
反射公共夹具八项通过，中庭候选四帧运行通过。
整体照明及原生探针激活关系仍待验收。
契约与证据见[局部反射接入](plans/2026-10-18-local-reflection-probe.md)。
原生循环行走已与待机合并，短路径操控验证通过。
角色在 180 帧回放中完成移动及动画状态切换。
来源、结果与限制见[操控验证](acceptance/nr/playable-integration.md)。
下一主线见[完整还原计划](plans/2026-10-17-native-render-restoration.md)。

投影头发支持可选的像素响应积分。
公共主次叶、条纹及照明组合已通过分项检查。
真实固定区域通过八相位高光能量门禁。
范围与运动待项见[积分验证](acceptance/nr/projected-hair-pixel-integral.md)。

单关节主带的移动与静止序列通过分项门禁。
响应编码及几何适配见[运动验证](acceptance/nr/material-motion-response.md)。
透明发丝、服饰及完整角色运动继续专项验收。

终末地专项已打通本机原生资源包读取。
原生待机候选已完成引擎五帧技术采样。
原生表情源包含 245 个控制项，绑定待实装。
帝江号中心切片已提取，并完成引擎联合技术采样。
场景支持实例编辑、保存重开与转换重放。
47 个场景材质已完成表面通道烘焙。
当前版本读取 232 个静态灯，联合候选使用 45 个形状灯。
颜色、GI 与反射的十二帧结果见[联合验证](acceptance/nr/joint-colour-lighting.md)。
53 个上层对象已进入[联合候选](acceptance/nr/upper-geometry.md)。
角色法线、金属度、发光及原生切线已接入候选。
原生玻璃已接入[场景颜色折射候选](acceptance/nr/native-glass.md)。
脸部双图 SDF 与专用着色保持待实施。
单个原生物流发射器已通过固定镜头运动检查。
通用刚体节点动画支持与角色蒙皮共同播放。
全部物流、原生相位及逐对象透明度仍待接入。
角色站位采用中庭前方平台的中轴线。
1080p、24 帧、16 秒展示录像已通过完整解码检查。
完整材质、衣摆效果与装载性能仍待验收。
展示进度见[帝江号材质与光照](acceptance/ev/dijiang-lighting-video.md)。
专项证据见[原生资产验收](acceptance/ev/native-assets.md)。

WC1 至 WC6 已完成，包含基础路径追踪和独立发布。
PT0 至 PT7 已完成，集中检查与画面已归档。
路径追踪支持金属高光、法线、发光面、环境和玻璃透射。
三个公开模型工程支持编辑、保存和独立发布。
打开步骤见[公开光追演示](tutorials/public-pathtracing-demos.md)。
实现范围见[本轮计划](plans/2026-10-15-pathtracing-completion.md)。

UX-A 至 UX-D 完成，UX-E 为 Ready。
自动检查已完成，新版使用体验等待集中确认。
交付入口与结果见[本轮交付](acceptance/ux/2026-10-14.md)。
范围与阶段记录见[交互优化计划](editor-usability-audit.md)。
用户已反馈人工复核基本可用，交互体验需要优化。

核对日期：2026-10-15。
路径追踪候选与产物指纹见[交付记录](acceptance/pt/2026-10-15.md)。
其余历史批次按各自记录中的候选解释。
本页回答：引擎现在能做什么，还有哪些问题，下一步做什么？

## 现在处于什么阶段

Windows 引擎、编辑器和独立 Player 提供制作与发布流程。
材质画布、二维内容、格式扩展及编辑器修复已交付。
最终验收的剩余条件由 P8 持有。

ED0 至 ED7 的开发交付已完成。
基础对象、层级菜单、组件、外部拖放及设置可用。
中文路径下的几何生成和脚本模板已修复。
P8 状态为 Ready，机器批次已收尾。
结果见[集中收尾](acceptance/p8/2026-10-14.md)。

安装版已通过 Monica 创建、重开和 GUI 发布回放。
发布目录移动后，原生游戏入口正常运行。
实体键鼠、听感和制作效率按人工复核记录。
当前开发平台为 Windows 桌面。

## 可以使用哪些能力

| 能力 | 当前可以做什么 | 使用范围与入口 |
| --- | --- | --- |
| 三维制作与发布 | 创建项目、编辑关卡、导入资产、运行预览、发布独立游戏 | [从空关卡制作并发布](tutorials/editor-first-game.md) |
| 材质画布 | 将类型化数值和纹理节点连接到一个材质输出。保存布局、撤销和重做 | 固定类型材质图。[材质制作](runtime/material-authoring.md) |
| 二维内容 | 显示矩形、精灵和中文文本。底层接口支持线段与投影标签 | 像素坐标文档及图元接口。[二维说明](runtime/scene-2d.md) |
| 资产交换 | 注册 29 种输入、20 种输出格式 | 每种格式有独立支持范围。[扩展格式](assets/extended-formats.md) |
| 音频关闭修复 | 设备通知排队，关闭时先结束通知任务 | 两种三十分钟长跑通过，听感待人工复核 |
| 移动交付 | 二维项目携带图片、中文字体和许可独立运行 | 两项目移动包通过。[本轮验收](acceptance/p8/2026-10-14.md) |

材质画布提供材质参数与纹理的连接表达，任意着色程序图属于后续扩展。二维内容通过 `.azure2d` 文档接入运行时，完整二维场景可视编辑器属于候选。GIF 读取首合成帧，PSD 读取合成图，SVG 栅格化，USD 接收受限 ASCII 静态网格。

## 这些结论有哪些证据

本轮两种配置各核齐 188 项非 GPU 标记检查。
首轮各有 186 项通过，两项修复后定向复测通过。
修复覆盖归档证据字节和并行脚本临时目录。
完整批次与定向复测分别保留原始日志。

材质参数、两个材质槽与实际视口像素变化已验证。
本轮两份二维发布包各含 488 个文件。
隔离运行显示中文标签与红色图元。
五个项目的双设备发布检查均通过。

标准与平台各三十分钟正式长跑通过。
十五轮性能的时间与内存预算均满足。
其中五轮未满足设备热状态条件。
完整性能门禁为十轮通过、五轮失败。

两种配置各核齐 73 项可执行图形检查。
原生输入检查被 Windows 前台激活拒绝阻断。
本轮候选、日志范围和待项见[集中收尾](acceptance/p8/2026-10-14.md)。
历史结果见[P9 集成验收](acceptance/p9/2026-10-12.md)。

## 哪些问题仍待确认

| 待项 | 已知结果 | 对开发的影响 | 后续处理 |
| --- | --- | --- | --- |
| 正式性能 | 本轮 10/15 轮通过，五轮热状态失败 | 本项验收待通过 | 热状态改善或相关机制修复后集中复核 |
| 三项历史归因 | 内存所有权、目录加载及重命名现场不足 | 归因结论待确认 | 同类失败复现时采集现场 |
| 实体操作与听感 | 自动回放已有结果，人工体验待确认 | 操作效率与主观体验待确认 | 按集中复核表记录真实使用 |
| 原生输入自动检查 | Windows 拒绝测试窗口获得前台 | 本项需可交互桌面 | 桌面允许前台焦点后复核 |

这些待项统一归属于[P8 延期清单](plans/p8-deferred-acceptance-manifest.json)。功能失败会阻塞依赖它的任务，环境和人工待项允许独立开发继续。项目状态应表达为“实现与自动检查已有结果，完整交付验收待完成”。

## 内容扩展的实现记录

本轮编辑器提交见[收尾索引](acceptance/p8/2026-10-14.md)。
职责与文件边界见[对象模型](architecture/editor-object-model.md)。

| 工作 | 提交 | 结果入口 |
| --- | --- | --- |
| 材质画布 U14 | `0d0dd0c` | [实施记录](acceptance/u14/2026-10-12.md) |
| 音频关闭修复 B1 | `181e6fe` | [实施记录](acceptance/b1/2026-10-12.md) |
| 格式扩展 G24 | `795c4d4` | [实施记录](acceptance/g24/2026-10-12.md) |
| 二维内容 G14 | `e0b04c2` | [实施记录](acceptance/g14/2026-10-12.md) |
| 集成与交付 P9 | `34d108a` | [实施记录](acceptance/p9/2026-10-12.md) |

阶段编号用于查找证据。阅读顺序为本页、[开发总计划](plans/azure-engine-plan.md)、[协作与验收](development-workflow.md)。历史候选的数字只解释其自身来源，历史入口见[计划快照](history/2026-10-12-planning/azure-engine-plan.md)。

混合细线采用共享覆盖积分及独立运行期策略。
相关细节、运动误差及完整响应共同验收。
裁剪候选和服饰边缘仍有专项失败。
范围见[混合覆盖验证](acceptance/nr/blend-temporal-coverage.md)。

裁剪纹理的自动层级按实际缩小过滤器估计覆盖。
可辨层级的 CPU、GPU 和最近邻兼容检查通过。
连续运动及组合采样仍有专项待验项。
结果见[缩小覆盖验证](acceptance/nr/alpha-mip-footprint.md)。

混合节点的当前运动覆盖与连续 Alpha 一致。
半透明度及渐隐循环通过相关细节和隐藏期检查。
共面叠层显露通过独立 HDR 历史检查。
真实角色的平移通过整体误差与能量检查。
高频细发丝幅度未通过，完整动态验收保持待项。
结果见[渐隐运动验证](acceptance/nr/blend-fade-motion.md)。
移动与显露见[历史验证](acceptance/nr/native-motion-history.md)。

裁剪表面的颜色、遮罩、深度壳及运动共用采样足迹。
透明运动外壳及部分响应组合通过所属 GPU 探针。
完整细线与服饰的质量门禁保持待项。
范围见[裁剪覆盖验证](acceptance/nr/cutout-motion-coverage.md)。

动态细节测量共用整体及三乘三高频相关幅度。
旋转四样点的离线对照通过头发、细线及轮廓门禁。
GPU 多样点着色、主辅视图及预算按专项计划接入。
证据见[动态细节验证](acceptance/nr/native-detail-sampling.md)。

RHI 的附件样本数、管线比例及能力查询通过双配置检查。
标准四样点插值与透明裁剪通过真实 GPU 数值验证。
主辅视图、材质及描边按四样点计划继续接入。
当前范围见[渲染硬件验证](acceptance/nr/multisample-rhi.md)。

[材质主 UV 绑定](acceptance/nr/material-primary-uv-policy.md)通过两配置、双设备的三十六组检查。
公开材质、阴影、覆盖、足迹正控制及失败清理通过，原始四帧摘要精确一致。
当前程序集为 858 份，实际相机与几何深度组合继续验收。

[实际网格位置](acceptance/nr/actual-world-depth-source.md)通过公开深度的完整区域来源核对。
两设备的可见量与原生位置控制逐字节一致，场景级投影继续接入。

[参考坐标帧与世界坐标深度](acceptance/nr/reference-position-frame.md)通过双配置、双设备核对。
公开世界坐标路径的完整深度平面与 CPU 预转换控制逐位一致，原始四帧摘要精确一致。
实际索引、朝向及 UV 组合继续独立验收。

[实际索引与正面方向](acceptance/nr/actual-geometry-index-source.md)通过双设备来源核对。
两份完整深度平面与来源控制逐位一致，实际 UV 组合继续验收。

[实际网格主坐标来源](acceptance/nr/actual-geometry-uv-source.md)完成双设备的实际位置、索引及 UV 组合核对。
完整深度平面及可见量与来源控制一致，后续接入统一场景视图。

[参考相机视图契约](acceptance/nr/reference-camera-view.md)通过两配置空间查询及双设备深度适配。
17017 条来源射线的往返误差满足八位子像素门槛，宿主视图选择与绘制绑定继续接入。

[参考光栅位置](acceptance/nr/reference-raster-projection.md)通过两配置、双设备数值核对。
组合保留设备最终深度及屏幕偏移，场景宿主绑定继续接入。

[项目参考视图配置](acceptance/nr/project-reference-views.md)通过版本、参数位值、保存重开及目录移动核对。
两配置项目契约及当前原四帧通过，场景选择继续接入。

[场景参考相机选择](acceptance/nr/scene-reference-camera-selection.md)通过持久锚点、刚性移动及优先级核对。
两配置十二组检查及当前原四帧通过，光栅与拾取消费继续接入。

[保守剔除参数](acceptance/nr/conservative-frustum-transport.md)通过两配置、两设备及旧模块兼容检查。
128 字节布局及原四帧通过，参考混合平面继续接入。

[参考光栅混合视锥](acceptance/nr/reference-raster-frustum.md)完成两配置、两设备检查。
有界参数、退化回退及原四帧通过，光栅与拾取继续接入。

[参考相机生产接入](acceptance/nr/reference-camera-render-binding.md)完成共享标定、光栅、拾取及编辑入口检查。
两配置、两设备的实际几何掩码和原四帧通过，完整外观按所属门槛继续验收。
