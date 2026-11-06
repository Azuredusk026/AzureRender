# 开发总计划

## 最终交付目标

交付可配置、可扩展、可组合的通用引擎。
编辑器与 Player 共享运行时、资产及渲染契约。
玩法规则通过项目组件、脚本及配置表达。
架构范围见[引擎实施计划](2026-10-06-engine-evolution.md)。

AI、脚本及界面通过统一生产操作观察和修改项目。
操作具有类型校验、事务及可追溯结果。

当前验收应用为完整帝江号中庭与原生角色。
场景包含围护、中央物流、照明、反射及动态遮挡。
角色支持眼睛、头发、皮肤、服装及原生动作。
可玩交付支持 WASD、碰撞及动作混合。

完成条件由既有门禁共同决定。
标定后的有效静态区域 SSIM 至少为 0.95。
关键区域的中位亮度误差目标至多为 10%。
局部外观、时间稳定性及资源释放按所属门禁验收。

交付须通过保存、重开、目录移动及独立 Player 运行。
探索项目与资产检视工具共同验证引擎复用。
标准负载的 GPU P95 至多为 16.6 毫秒。
Player 的 CPU 工作 P95 至多为八毫秒。

性能负载、双设备和长跑按正式阶段验收。
全部适用条件通过后，才能登记最终完成。

## 当前执行顺序

执行采用[任务预算与调度](../development-workflow.md)。
画质单元优先，每次只验证一个原因。
定位十分钟，候选修改与短检二十分钟。
探索达到三十分钟后登记结论，并接续独立任务。

| 顺序 | 任务单元 | 下一份交付物 | 核验及启动条件 |
| --- | --- | --- | --- |
| 1 | 脸部肤色 | 当前暖色局部与同机位联合画面 | 使用实际因子，保留用户外观待验收 |
| 2 | 头发及角色阴影 | 固定姿态的单变量图像对照 | 沿用完整区域及原始像素门槛 |
| 3 | 地面反射及间接照明 | 同机位分项对照及明确差异范围 | 固定粗糙度、曝光、输入与时间 |
| 4 | 联合动态 | 静态候选的角色与物流序列 | 当前静态候选具备局部通过证据 |
| 待调度 | 编辑器资产网格超时 | 闲置管线生命周期的单项结果 | 画质核验完成，或超时阻断当前任务 |

资产网格超时已保留根因采样及失败日志。
管线复用为[隔离候选](2026-11-05-pipeline-reuse.md)。
综合等待与短检失败已归档，重试需取得异常栈。
生产构建保持受检实现，原十秒预算继续有效。

相机精度的[单变量分项](../acceptance/nr/outline-snapshot-order.md)完成。
私有候选仍有 1,057 个头发像素超出一字节。
生产接口保持受检基线，严格字节检查继续保留失败。
当前 GI 的[启停分项](../acceptance/nr/scene-material-quality-diagnosis.md)已确认地面和墙面响应。
场景反射与最终照明外观继续核验。
当前开发和冻结交付入口见[完整场景显示](../tutorials/restored-native-scene.md)。

联合性能按[渲染架构调研](../research/2026-11-05-render-performance-architecture.md)推进。
稳定身份、帧数据索引及 GPU 事件分解为当前执行范围。
资源键候选的四帧及 535 份生产程序逐字节一致。
实体身份直接查询通过两配置各十九项检查。
四帧冻结对照一致，详见[身份验证](../acceptance/nr/runtime-node-identity.md)。
交流供电下 CPU P95 为 38.30 毫秒，GPU 为 40.38 毫秒。
运动向量绑定的[双设备验证](../acceptance/nr/motion-bindings.md)通过。
实际管线绑定为 209 次，重复绑定为零。
该候选 CPU P95 为 33.85 毫秒，GPU 为 38.94 毫秒。
帧场景[快照复用](../acceptance/nr/frame-context-snapshot.md)通过两配置各九项检查。
该候选四帧输出一致，性能采集待空闲 GPU。
相机与场景共享输入通过两配置各十四项 CPU 检查。
扩展回归及延期范围见[共享输入验证](../acceptance/nr/render-camera-snapshot.md)。
外部游戏退出后恢复[设备及原生复测](../acceptance/nr/idle-gpu-validation.md)。
两配置双设备各三项检查通过，原生四帧与冻结参考一致。
构建目标选择通过[两配置增量核验](../acceptance/recovery/scoped-build-provenance.md)。
Debug 目录规模的[原用例](../acceptance/recovery/directory-scale-diagnosis.md)本轮 67.95 秒通过。
历史超时和隔离候选保留原证据，耗时差异仍需归因。
缓冲依赖的[批量入口](../acceptance/nr/buffer-barrier-batching.md)通过两配置各七项 CPU 检查。
双设备的蒙皮与运动机制检查通过，原生收益按本轮范围记录。
节点[快照复制](../acceptance/nr/runtime-snapshot-copy.md)通过两配置各十五项 CPU 检查。
本轮 CPU 工作 P95 为 24.83 毫秒，GPU 为 39.81 毫秒。
混合场景的[透明临时分配](../acceptance/nr/opaque-transparency-scratch.md)通过两配置各十二项检查。
不透明顶点扩容时，候选的逐帧分配保持稳定。
阴影[接收分项](../acceptance/nr/shadow-receiver-cost.md)保留热状态条件失败。
[同帧照明原型](../acceptance/nr/opaque-lighting-reuse.md)通过像素与分支检查，收益未成立。
完整覆盖阴影的[纯深度提交](../acceptance/nr/depth-only-shadow.md)通过两配置各十二项 CPU 检查。
四组双设备深度检查与原生四帧冻结对照通过。
短时 GPU 帧 P95 为 33.93 毫秒，阴影为 4.50 毫秒。
短时环境检查通过，正式帧预算保持原标准。
当前原生的[CPU 调用栈](../acceptance/nr/current-cpu-hotspots.md)已取得实际模块与符号来源。
相机及呈现的[物化输入共享](../acceptance/nr/materialized-camera-input.md)通过两配置各十四项 CPU 检查。
原生四帧冻结对照一致，短时成对 CPU 均值降低约百分之 6.06。
三次环境检查通过，候选 CPU 工作 P95 为 21.79 毫秒。
资源解析与关卡轮询继续独立定位。
私有[阶段成本](../acceptance/nr/cpu-stage-cost.md)定位到实例准备及透明排序。
透明[世界坐标候选](../acceptance/nr/transparent-world-position.md)通过四帧兼容，成本比较受热状态限制。
生产源码及代码段恢复为候选前字节，两配置各十二项回归通过。
[静态变形输出复用](../acceptance/nr/static-skinning-output.md)通过两配置各十四项 CPU 检查。
十二项双设备 GPU 检查及原生四帧冻结对照通过。
实际帧复用 742,213 个顶点请求，动态输入继续生成。
短时 CPU 工作 P95 为 21.74 毫秒，GPU 帧为 35.09 毫秒。
单候选计时的热状态未通过，整帧因果收益资格未成立。
实例准备的[子阶段观察](../acceptance/nr/instance-preparation-cost.md)取得一百条同帧记录。
节点分组均值为 2.20 毫秒，姿态采样为 2.42 毫秒。
[稳定资源分组](../acceptance/nr/stable-instance-grouping.md)通过两配置各十五项 CPU 检查。
实际联合场景四帧与原冻结参考逐字节一致。
组内源节点顺序及连续实例身份保持一致。
单候选短时 CPU 工作 P95 为 32.92 毫秒，GPU 为 34.70 毫秒。
软件热降频触发环境失败，整帧收益资格未成立。
[加载资源绑定姿态](../acceptance/nr/loaded-bind-pose.md)通过两配置各十九项 CPU 检查。
十二项双设备 GPU 检查及原生四帧冻结对照通过。
实际末帧有 860 个实例按值复制绑定姿态。
短时 CPU 工作 P95 为 20.68 毫秒，GPU 为 33.77 毫秒。
软件热降频及热余量不足触发环境失败，整帧收益资格未成立。
阴影[深度范围分层](../acceptance/nr/shadow-depth-hierarchy.md)保留为隔离候选。
候选通过两配置各二十项 CPU、二十四项双设备 GPU 及四帧冻结对照。
单候选 GPU 帧均值为 32.21 毫秒，热状态未通过。
正式运行输入按受检单层范围的来源核对。
[加载资源身份映射](../acceptance/nr/loaded-resource-identities.md)通过两配置各二十项 CPU 检查。
十二项双设备 GPU 检查及四帧冻结对照通过。
一千零二十四个未使用别名在两种描述符路径均增加零字节逐帧分配。
兼容场景复用按当前身份刷新映射，实例解析及可选对象检查通过。
单候选 CPU 工作 P95 为 30.29 毫秒，GPU 为 33.96 毫秒。
热余量条件失败，整帧收益资格未成立。
[关卡刷新数据释放](../acceptance/nr/level-refresh-retirement.md)通过两配置各二十八项 CPU 检查。
三轮未变化刷新中的主线程大块释放均为零，持有量稳定。
原生四帧冻结对照一致，未变化的底层 GPU 产物引用既有检查。
单候选 CPU 工作 P95 为 19.80 毫秒，GPU 为 35.13 毫秒。
热状态未通过，整帧收益资格未成立。
[完整覆盖深度预填充](../acceptance/nr/covered-depth-prefill.md)通过两配置各三十二项 CPU 检查。
两配置双设备的深度及颜色读回检查通过。
原生四帧冻结对照一致，淡出及四样本保持覆盖规则。
单候选 CPU 工作 P95 为 20.47 毫秒，GPU 为 34.45 毫秒。
热状态未通过，整帧收益资格未成立。
[当前 GPU 事件分解](../acceptance/nr/current-gpu-events.md)取得实际运行产物的捕获。
重放计时及材质查询均核对冻结输入。
受检主颜色通用程序的七十一个事件合计 11.84 毫秒。
普通不透明[完整覆盖专用常量](../acceptance/nr/opaque-coverage-specialization.md)保留为隔离原型。
两配置各三十二项 CPU 及二十四项双设备 GPU 检查通过。
原生后两帧分别有九个及七个像素相差一字节。
短时 GPU 帧 P95 为 27.65 毫秒，热状态检查通过。
原型未通过零像素差条件，整帧收益资格未成立。
接续[提前深度测试](2026-11-06-ordinary-early-depth.md)的独立程序验证。
完整 CPU、GPU 和热状态预算继续保留失败。

正式检查的[构建与设备入口](2026-11-05-acceptance-preflight.md)已经恢复。
两种配置各八项定向检查通过，历史字节归因按原范围保留。
Intel 的整图稳定性仍有少量像素差异。
编辑预览与正式测量按各自依赖接续。

材质[照明接收](../acceptance/nr/material-lighting-reception.md)通过两配置公共回归。
匹配输入的三十六个不透明区域均通过静态门槛。
原冻结独立对照的服饰二、五仍有失败项。
原生服饰变体、完整角色及场景反射继续执行 Q1。

状态由本页统一管理，专项文件提供任务和证据。
当前优先级为画质恢复、联合验证和可玩交付。
阶段标识用于执行与证据索引。

联合画质验收未通过，依据为用户的实际画面复核。
当前缺陷包括角色阴影、头发着色及 KK 高光。
皮肤色带、场景反射与间接照明同属优先修复项。
独立角色与独立场景分别提供外观参照。

操控、保存及独立运行结果仅证明对应功能。
联合画质须按相同输入核对材质、照明与显示条件。
当前执行聚焦五项外观缺陷，随后接续受影响回归。
同机位实验与场景来源参数见[输入核验](../acceptance/nr/joint-visual-input-isolation.md)。

接收范围内的主光及阴影来源通过[机制验证](../acceptance/nr/receiver-directional-light.md)。
默认近景、固定全景及隐藏环境保持严格兼容。
角色参考灯及显示拟合保持诊断状态。
联合外观继续执行用户确认的五项缺陷验收。

脸部与头发的全部顶点已按原生骨骼姿态核对。
同条件材质对照保留接近参考的头部效果及边缘差异。
固定位置与可玩出生点均在最细辐照层内部。
完整数值及范围见[材质与姿态核验](../acceptance/nr/native-material-input-audit.md)。

同曝光的[照明分项](../acceptance/nr/joint-lighting-components.md)显示角色环境贡献偏强。
受测区域的直接漫反射中位量为零，墙面控制保持一致。
角色主光、阴影与源曝光换算继续组合验证。
换算契约按[材质辐亮度计划](2026-11-01-material-radiance-normalization.md)实施。

完整线性倍率已通过两配置各三十六次 GPU 采集。
十类材质的独立曝光对照均逐像素一致。
眼睛公开像素参考及乘法覆盖的单位边界通过检查。
原生联合外观继续按[辐亮度验证](../acceptance/nr/material-radiance-normalization.md)核对。

原生五个角色内部区域已通过线性曝光等价检查。
材质声明场景或线性显示策略，场景保留原生调色。
联合候选中对应区域的亮度误差为零至 1.74%。
五个矩形的静态 SSIM 最低为 0.99867。

六组原生采集及受影响回归见[显示验证](../acceptance/nr/material-display-transform.md)。
隐藏环境十二帧及缺省近景逐像素兼容。
完整局部外观及正式门禁继续执行，画质状态保持未通过。

联合可玩候选沿用上述材质与照明契约。
两配置各完成 540 帧及十三项输入检查。
普通 Player 的三帧与回放入口逐像素一致。
实际组合及原生响应见[组合验证](../acceptance/nr/joint-playable-display.md)。

KK、脸部阴影和色带具有非零的原生区域响应。
对象阴影在颈部、胸部及肩部通过启停对照。
原独立照明与移除环境几何的控制分别核验。
上述检查保持声明范围，整体验收继续执行。

含雾关卡的[重开复用](../acceptance/recovery/view-fog-resource-reuse.md)通过两配置回归。
各 540 帧完整路线及十二帧固定全景逐像素兼容。
每条完整路线仅执行一次渲染器 CPU 准备。
加载复用结果按其范围归档，联合画质继续未通过。

[全身动态对照](../acceptance/nr/full-body-native-comparison.md)确认衣料仍有明显照明差异。
不透明服饰二、五的中位亮度误差超过 110%。
单变量采集确认环境辐照及反射对差异有明显贡献。
继续核对衣料原生输入及消费，联合画质保持未通过。

[原生衣料输入](../acceptance/nr/native-cloth-inputs.md)核验五次绘制的公共高光色带。
五份服饰的八个片元通过[视角色带合成核验](../acceptance/nr/npr-view-ramp-composition.md)。
公共[颜色合成接入](../acceptance/nr/npr-ramp-runtime.md)通过两配置回归。
四组全身运动采集的三十六个匹配区域通过静态门槛。

原冻结服饰对照及联合画质继续保留失败状态。

[角色参考核验](../acceptance/nr/portrait-input-recovery.md)完成原始双图及动态近景采集。
当前程序可产生原始入口的角色外观。
联合输入的完整头发、阴影和皮肤继续验收。
头发边缘按[独立裁剪图计划](2026-11-02-independent-coverage.md)修复。

[独立裁剪图接入](../acceptance/nr/material-coverage.md)通过两配置公共检查。
来源层级、扩张深度、描边与错误退出按同一契约核验。
六个原生强边缘光点均在一字节误差内。
生成层级通过[覆盖保持验收](../acceptance/nr/material-coverage-mips.md)。
完整头发及场景画质继续执行 Q1。

原生裁剪图的[联合验证](../acceptance/nr/native-coverage-integration.md)完成四百帧采集。
匹配输入的三十六个全身区域及二十个近景矩形通过检查。
原冻结服饰一、二、五及完整头发继续保留失败项。
角色阴影、皮肤、高光及场景反射继续按实际参考修复。

角色展示按[动态验收](../acceptance/nr/joint-conveyor-motion.md)使用约 3.82 米站位。
模型缩放为一，完整环境及相机声明保留。
对象阴影的完整头发区域仍有 73 个像素超过一字节。
场景可见性及时间采样对照保存独立的实际指标。

展示站位、中央物流及角色待机按[动态计划](2026-11-03-joint-scene-dynamics.md)推进。
执行顺序为静态构图、物流运动、角色循环及联合回归。
画质、动态功能和性能分别保留验收状态。

原生待机按[循环验收](../acceptance/nr/joint-idle-loop.md)保留来源并追加闭合片段。
八秒联合录像包含三次完整待机循环及物流运动。
本轮[动态回归](../acceptance/nr/joint-dynamics-regression.md)通过两配置受影响检查。
项目重开的前四帧与原采集逐像素一致。

原生材质按[组合计划](2026-11-03-native-material-composition.md)核对固定姿态。
衣料 Y 分量按[法线核验](../acceptance/nr/native-normal-orientation.md)修正。
三处受测衣料的相似性提高，服饰一、二继续验收。
头发及脸部的对应控制保持逐像素一致。

法线方向的[联合回归](../acceptance/nr/joint-normal-orientation.md)完成八秒采集。
两张图像之外的模型、文本和动画状态保持一致。
右墙的一像素一字节差异保留严格兼容失败状态。
完整衣料、头发、皮肤及场景反射继续执行 Q1。

材质覆盖层的[深度策略](../acceptance/nr/material-overlay-depth.md)通过两配置受影响回归。
198 帧核验同平面覆盖、遮挡、混合及场景雾。
原生服饰的来源层级及特效组合继续验收。
联合画质保持未通过，随后接续 Q2 至 Q4。

覆盖图元按[工具验收](../acceptance/nr/material-overlays.md)保留资产结构。
三处匹配的不透明衣料区域通过既定门槛。
完整输出的服饰二及透明材质继续验收。
八秒联合候选保留十四像素的严格重开失败。

透明衣料的深度行为按[专项计划](2026-11-03-blend-depth.md)执行。
版本二画像声明深度写入及同深度比较方式。
普通薄片与衣料分别核验实际遮挡和缺省兼容。
固定姿态的完整角色区域继续按既定门槛验收。
公共深度配置通过[专项回归](../acceptance/nr/blend-depth.md)。
角色的透明深度候选保留完整区域失败记录。

表面发光按[覆盖计划](2026-11-03-opaque-surface-overlays.md)进入反射快照。
公共机制通过[覆盖验收](../acceptance/nr/opaque-surface-overlays.md)。
本轮八秒候选的四帧项目重开逐字节一致。
完整角色、反射及间接照明继续按原门槛验收。

透明衣料按实际程序使用十五号纹理槽。
服饰二、五的指定片元通过既有高光公式核验。
完整材质接入、联合外观及运动稳定性继续验收。

仅替换原生调色表的[单变量核验](../acceptance/nr/material-display-domain.md)已完成。
五个受测区域仍出现约 16% 至 44% 的亮度变化。
部分眼睛颜色具有调色表的不可达边界证据。
显示覆盖及时间历史按[材质显示变换](2026-11-01-material-display-transform.md)验收。

| 阶段 | 交付与通过条件 | 当前状态 |
| --- | --- | --- |
| Q0 参考输入锁定 | 项目、材质、程序及采集参数可校验 | 已完成 |
| Q1 角色与反射画质 | 同条件核对眼睛、发丝、服装及地面倒影 | 执行中 |
| Q2 联合场景 | 完整环境、角色、动作和动态照明通过回归 | 执行中 |
| Q3 可玩交付 | 输入、碰撞、保存重开、目录移动及发布通过 | 功能候选通过 |
| Q4 正式验收 | NR0 至 NR12 全部适用门禁及 P8 条件通过 | 待完成 |

脸部遮罩轮廓光按[实施计划](2026-11-04-face-masked-rim.md)验证。
原生片元追踪确认头部朝向、透明度及宽度混合。
联合镜头完成 192 帧回归，角色和物流共采样 2,112 次。
普通入口通过全项目检查，起始四帧与[衣料描边联合序列](../acceptance/nr/evidence/2026-11-04/sampled-outline/motion.json)逐字节一致。
两种构建分别完成[可玩路线](../acceptance/nr/evidence/2026-11-04/joint-playable-route/results.json)的 540 帧输入和碰撞核验。
可玩候选完成编辑器保存、目录移动和独立发布。
隔离源工程和引擎后，游戏程序以包内资源完成 540 帧路线，三帧默认画面逐字节匹配。
发布包包含 14 个生效的角色材质槽及 33 层场景反射图集。
皮肤材质按肤色核验采用暖色基础颜色因子，Debug 与 Release 的可玩候选均完成 540 帧路线。
候选指纹见[可玩交付记录](../acceptance/nr/evidence/2026-11-04/skin-warm-delivery/results.json)。
屏幕法线按[专项核验](../acceptance/nr/screen-surface-normal.md)完成公共机制验证。
两种构建、三种采样配置共通过八十四项 GPU 检查。
暖肤色联合镜头的八帧输出逐像素一致。
程序化视角遮罩与影色饱和度见[颜色核验](../acceptance/nr/npr-view-mask.md)。
两种构建及两种采样配置完成 56 次独立颜色检查。
完整轮廓的阴影可见度按实际输入继续核验。
屏幕法线的偏置通过[独立阴影核验](../acceptance/nr/screen-normal-shadow.md)。
两种构建及三种采样配置完成 36 次图像检查。
受光描边与匹配背景完成[固定姿态核验](../acceptance/nr/lit-surfaces.md)。
服饰一、二的区域相似度为 0.96499、0.96409。
完整头发及后续阶段的区域相似度为 0.98887。
受光表面的[联合序列](../acceptance/nr/evidence/2026-11-05/lit-surfaces-motion/results.json)完成 192 帧采集。
Debug 重开的四帧与 Release 逐字节一致。
两种构建各完成 540 帧可玩输入回放。
受光表面的[独立交付](../acceptance/nr/lit-surfaces-delivery.md)检查通过。
移动目录重开的三帧逐字节一致。
隔离源工程和安装目录后完成 540 帧路线。
完整着色目录及独立编辑恢复见[工作流核验](../acceptance/recovery/shader-catalog-reload.md)。
两种构建的 535 份输出与常规构建逐字节一致。
Debug 综合交互保留资产刷新超时待项。
严格字节及完整联合画质继续核验。
快照的深度壳顺序及背景法线见[来源核验](../acceptance/nr/outline-snapshot-order.md)。
图像准备保持原始字节，两配置联合四帧逐字节一致。
Debug 联合回归在九百秒预算内完成，资源正常卸载。
完整角色的皮肤、发丝、描边及透明阶段继续执行 Q1。
服饰一、二的描边覆盖见[固定姿态核验](2026-11-03-character-reference-stages.md)。
虹膜按[眼部输入](../acceptance/nr/eye-npr-inputs.md)核验独立基准和联合动作。
地面倒影按[固定箱体对照](../acceptance/nr/scene-material-quality-diagnosis.md)核验备份机位的十二帧输出。

Q0 至 Q4 汇总当前执行顺序及完成证据。
正式范围和阈值由 NR、引擎交付及 P8 计划持有。
严格像素兼容和既有失败记录继续保留。
阶段之间按自主实施规则连续推进。

Q0 的两组真实输入及错配拒绝见[校验记录](../acceptance/recovery/evidence/2026-10-31/reference-input-lock/summary.json)。
参考清单的具体约束见[参考基准](../acceptance/nr/reference-baseline.md)。

每份校验结果对应记录的程序及输入摘要。
构建候选使用其实际采集记录建立独立清单。

## 结构与实现约束

生产代码沿用现有目录、模块边界及 CMake 目标。
渲染机制属于 RenderCore，配置和运行装配属于 Runtime。
项目内容及离线转换持有角色、关卡和来源身份。
编辑器、Player 和自动工具共用生产接口。

公开夹具与私有实景分别验证通用性及实际外观。
来源清单记录项目、资产、程序、参数及输出摘要。
原始资产、场景配置和生成结果按各自规则保留。
源码、说明、测试及产物职责见[源码导航](../development/source-navigation.md)。

## 本机恢复与接续

当前工作目录为 `D:/Projects/FYP`。
环境清单与开发顺序见[恢复计划](2026-10-30-development-recovery.md)。
工具链、运行资产及双配置公共验证已完成。
初始化归因与环境像素兼容继续复核。

完整物流场景按原机位重渲十二帧。
内容、地面反射与少量像素差异见[画面恢复](../acceptance/recovery/full-scene-visual.md)。
完整场景及角色联合候选分别维护画质基准。

完整环境与原生角色的同屏画面见[联合验证](../acceptance/recovery/full-scene-character.md)。
固定位置及待机已验证，严格环境像素兼容继续复核。

当前材质功能位及主辅视图见[特化验证](../acceptance/nr/material-class-feature-specialization.md)。
资产常驻索引、双配置回归及启动边界见[索引验证](../acceptance/recovery/asset-dependency-index.md)。

共享启动快照及双配置完整入口见[启动验证](../acceptance/recovery/project-startup-context.md)。
完整 Debug 四样点在六百秒内完成八帧和卸载。

后续接续主场景预算、原版射线与动态画质。
原生射线的冻结数值参考见[顺序验证](../acceptance/nr/native-ray-arithmetic-order.md)。

Vulkan 三份完整输出及双配置回归见[参考验证](../acceptance/nr/native-ray-vulkan-reference.md)。

具名投影射线、独立槽位及完整来源结果见[公共接口验证](../acceptance/nr/projected-ray-contract.md)。
主场景接续深度、运动编码和前帧资源装配。

引擎相机、线性运动及镜面控制见[相机适配验证](../acceptance/nr/projected-camera-adapter.md)。
表面快照通过公共深度和法线准备接口转换。

公共表面转换及镜面组合见[准备验证](../acceptance/nr/projected-surface-preparation.md)。
公共生产与过滤组合见[组合验证](../acceptance/nr/projected-reflection-composition.md)。
完整着色清单及混合编译见[契约验证](../acceptance/recovery/shader-compiler-contract.md)。

主场景类型化选择及当前帧生产见[场景验证](../acceptance/nr/projected-main-scene.md)。
接续投影历史的实际运动、采样抖动及成功提交身份。

公共 UV 校正和运动响应见[运动验证](../acceptance/nr/projected-history-motion.md)。
主场景成功提交、取消及实际采样见[历史装配](../acceptance/nr/projected-main-history.md)。
完整环境和角色分别验证，投影足迹清晰度继续核对。

当前优先处理地面反射及角色近景画质。
证据及定位顺序见[画质核验计划](2026-10-31-projected-footprint-quality.md)。
内容同屏、功能检查及外观验收分别记录。

原始正面角色图已按独立项目重跑三十三帧。
入口差异与剩余像素条件见[角色参考](../acceptance/recovery/character-reference.md)。
联合画面继续按相同材质与颜色来源校正。

完整照明候选的隐藏环境十二帧严格兼容。
参数、近景及时间采样见[照明联合验证](../acceptance/nr/character-illumination-integration.md)。
无光照雾图层的接口配对按既有渲染模块验证。
Q1 的整体外观和时间稳定性继续验收。

动态角色正面完成完整画面、照明候选及底色采集。
同机位可辨认虹膜、竖瞳、纹理及高光。
远景眼部的几何尺度及发冠图案继续按来源核对。

动态头发的主坐标已按冻结参考恢复。
恢复工具、正面图案及回归见[坐标验收](../acceptance/nr/hair-source-uv.md)。
九段动画、327 个节点和原二进制前缀保持完整。

十三份画像的主坐标及恢复前置条件完成盘点。
身体、服饰及眉毛按各自来源边界继续验收。

完整场景的独立操控检查见[联合验证计划](2026-10-31-full-scene-playable.md)。
该检查复用已有角色、物理及相机系统。

完整场景的两配置输入、动作及重开回放已通过。
护栏持续阻挡、松键静置及固定画面通过独立检查。
结果及范围见[操控验收](../acceptance/recovery/full-scene-playable.md)。
整体画质、性能及发布仍按 Q1 至 Q4 的条件验收。

发布工具支持有界的离线项目校验预算。
Windows 安装路径及隔离 Python 通过实测。
范围与证据见[发布校验](../acceptance/recovery/game-package-validation.md)。

完整场景的保存、中文目录迁移及独立入口已通过机器检查。
默认入口三帧与工程入口逐字节相同。
原生入口完成默认 1080p 的完整输入回放。
交付位置及边界见[独立运行](../acceptance/recovery/full-scene-delivery.md)。

## 代码检索与证据管理

有界检索、证据归档与按需符号地图已完成。
操作与范围见[源码导航](../development/source-navigation.md)。
四十项工具回归与隔离运行见[专项验收](../acceptance/retrieval/2026-10-27.md)。

## 原生帧分析与还原

已按用户指令恢复自主实施，持续推进 NR0 至 NR12。
截帧资源的明确来源选择见[来源验收](../acceptance/nr/scene-source-selection.md)。
查询像素原点及场方向见[坐标验收](../acceptance/nr/irradiance-query-frame.md)。
当前继续核对完整照明、连续采样及中央物流。

独立 UV1 与统一表面输入见[坐标验收](../acceptance/nr/surface-uv-domain.md)。
来源法线解码及分项镜头见[法线验收](../acceptance/nr/lit-normal-decoding.md)。
作者法线和两个生产入口见[角点验收](../acceptance/nr/mesh-corner-normals.md)。
局部灯来源倍率与完整镜头见[绑定验收](../acceptance/nr/local-light-profile-binding.md)。

活动主光辐照输入及方向核对见[主光验收](../acceptance/nr/main-light-runtime-binding.md)。
级联上游遮挡和滤波尺度见[深度验收](../acceptance/nr/cascade-caster-depth.md)。
局部灯颜色空间与温度分支边界见[颜色验收](../acceptance/nr/local-light-color-space.md)。
活动颜色表和后处理来源见[调色验收](../acceptance/nr/captured-color-grading-lut.md)。

活动索引和实际灯光快照见[灯光验收](../acceptance/nr/captured-active-lights.md)。
静态作者法线与精确轴置换见[批量验收](../acceptance/nr/static-authored-normals.md)。
上方与后方来源接入见[围护验收](../acceptance/nr/enclosure-authored-normals.md)。
物流图层、完整材质与实际绘制见[图层接入](../runtime/unlit-texture-layers.md)。

活动物流参数及实际资源见[截帧绑定](../acceptance/nr/captured-vfx-layers.md)。
层级采样、覆盖率及实际实例见[绘制验收](../acceptance/nr/vfx-mip-and-coverage.md)。
近远淡出及实际图层见[深度验收](../acceptance/nr/vfx-view-depth-fade.md)。
公共顶点颜色、多流绑定及实际特效见[颜色验收](../acceptance/nr/vertex-color-streams.md)。

柱面视角覆盖率与加色合成见[合成验收](../acceptance/nr/view-dependent-vfx-coverage.md)。
实际箱体姿态、共享材质及完整镜头见[箱体验收](../acceptance/nr/captured-logistics-boxes.md)。
独立覆盖率、作者层级及实际字形见[覆盖率验收](../acceptance/nr/unlit-coverage-texture.md)。
三项原生材质组合与隔离绘制见[组合验收](../acceptance/nr/remaining-vfx-combinations.md)。

基础坐标扰动、独立采样及实际墙面见[扰动验收](../acceptance/nr/base-uv-distortion.md)。
中央字形柱与独立覆盖率组合见[字形柱验收](../acceptance/nr/central-glyph-composition.md)。
雾效传输、体积格式及原版载荷见[雾效数据验收](../acceptance/nr/fog-transport-data.md)。
三维雾场上传、原版采样及资源释放见[雾场 GPU 验收](../acceptance/nr/fog-volume-gpu.md)。

像素原点、抖动及深度映射见[视图坐标验收](../acceptance/nr/view-fog-projection.md)。
场景资源、捕获视图及保存重开见[绑定验收](../acceptance/nr/view-fog-scene-binding.md)。
预积分雾场、实际表面与资源隔离见[表面验收](../acceptance/nr/view-fog-surfaces.md)。
中央白色标识与浮点附件对照见[标识验收](../acceptance/nr/central-late-glyph.md)。

逐采样深度、覆盖率与双配置回归见[轮廓验收](../acceptance/nr/view-fog-multisample.md)。
普通无光照表面、附件一致性及 GPU 成本见[预算验收](../acceptance/nr/unlit-opaque-budget.md)。
光谱大气、远端片元与公共数值见[解析雾验收](../acceptance/nr/analytic-fog.md)。
场景支持[解析雾配置](../runtime/scene-fog.md)及独立解析介质。

双配置、真实片元与完整候选见[合成验收](../acceptance/nr/scene-fog.md)。
阶段截图、机位和待验差异见[画面记录](../acceptance/nr/visual-history.md)。
物流彩色环带的源实例及采样见[还原验收](../acceptance/nr/opaque-logistics-rings.md)。
公共 glTF 采样器及完整场景结果见[采样器验收](../acceptance/nr/gltf-sampler-contract.md)。

原版动态反射过滤及公共接入边界见[来源验收](../acceptance/nr/reflection-filter-source.md)。
独立过滤资源及双配置结果见[公共过滤验收](../acceptance/nr/reflection-spatial-filter.md)。
启动预算包含隐藏资源引用及材质解码的驻留峰值。
接续入口为[原生帧还原交接](2026-10-22-native-render-restoration-handoff.md)。

九份 D3D11 原生帧已通过官方 1.46 加载和通道查询。
中庭与莱万汀各一帧完成 2560×1440 实际图像导出。

文件哈希和重放范围见[原生帧验证](../acceptance/nr/native-game-captures.md)。
双锚帧的完整事件与管线索引已通过检查。
范围见[绑定索引](../acceptance/nr/native-binding-index.md)。
随后核对角色、场景照明、地面反射和物流的实际绑定。

相机版本二与真实子像素输入见[投影验证](../acceptance/nr/camera-projection-offset.md)。
NR0 的参考门禁及 NR1 至 NR12 的正式条件保持待验。

NR7-AA0 已核对原生时序输入、运动与历史 RGB 合成。
单帧来源及连续序列边界见[时序核验](../acceptance/nr/native-temporal-inputs.md)。
AA1、AA2 的提交基础与运动数学已完成公共验证。
运动、前帧变形及 HDR 历史已接入主辅视图。

双配置与真实角色序列见[接入验证](../acceptance/nr/temporal-reconstruction-integration.md)。
继续执行 AA4 至 AA6 的细节、运动与预算门禁。
覆盖、显式镜头及公共四倍尺寸结果见[质量验证](../acceptance/nr/temporal-coverage-quality.md)。
描边输出像素与细线能量见[采样验证](../acceptance/nr/outline-temporal-footprint.md)。

高光采样输入及诊断曝光见[分量核验](../acceptance/nr/component-radiometry.md)。
投影头发的积分契约与固定机位结果见[像素积分](../acceptance/nr/projected-hair-pixel-integral.md)。
不透明主带运动与部分响应见[运动验证](../acceptance/nr/material-motion-response.md)。
混合细线积分与细节门禁见[覆盖验证](../acceptance/nr/blend-temporal-coverage.md)。

裁剪纹理的生成足迹见[缩小覆盖验证](../acceptance/nr/alpha-mip-footprint.md)。
混合节点透明度与运动覆盖见[渐隐验证](../acceptance/nr/blend-fade-motion.md)。
真实角色移动与共面显露见[历史验证](../acceptance/nr/native-motion-history.md)。
裁剪足迹与运动外壳见[覆盖验证](../acceptance/nr/cutout-motion-coverage.md)。

动态高频门禁及几何样点对照见[细节验证](../acceptance/nr/native-detail-sampling.md)。
多样点 RHI 声明及设备验证见[采样契约](../acceptance/nr/multisample-rhi.md)。
角色四样点动态质量见[材质验证](../acceptance/nr/multisample-character-material.md)。
深度副本及外壳覆盖见[副本验证](../acceptance/nr/multisample-snapshot-outline.md)。

副本着色需求及真实图像一致性见[成本验证](../acceptance/nr/snapshot-depth-demand.md)。
四样点主通道成本与联合画质继续按 MS5 验收。
主通道预填及双配置动态质量见[专项验证](../acceptance/nr/opaque-main-depth-prefill.md)。
完整资产缓存复用与主入口图像见[缓存验证](../acceptance/nr/complete-asset-cache-reuse.md)。

主场景的实际绘制成本与剩余预算见[专项计划](2026-10-25-main-scene-shading-budget.md)。
普通材质的有界专用化及快照精度见[专项验证](../acceptance/nr/ordinary-material-specialization.md)。
基础功能集合与共享头文件构建见[功能验证](../acceptance/nr/generic-material-feature-folding.md)。
副本预填、实际入口及冻结图像见[专项验收](../acceptance/nr/opaque-snapshot-depth-prefill.md)。

完整画像的轻量槽位查询见[专项验收](../acceptance/nr/material-slot-inspection.md)。
主入口的完整画像、眼睛坐标及动态轴见[接入验证](../acceptance/nr/main-character-integration.md)。
独立镜头平移及真实角色细节见[镜头验证](../acceptance/nr/camera-motion-quality.md)。
公共源目标及最近深度解析见[目标验收](../acceptance/nr/multisample-scene-target.md)。

主辅视图的实际样本、质量与回退见[视图验收](../acceptance/nr/multisample-view-quality.md)。
完整运动、透明覆盖和服饰闪烁继续执行 AA4 至 AA6。

鼻下与脸颊暗影按直接光、GI、遮罩和镜面分项核验。
脸部径向法线候选通过双配置各 36 次 GPU 检查。
单姿态头骨与原生预留帧已核对。
真实角色近景通过，跨姿态与完整照明继续验收。

范围见[脸部法线验收](../acceptance/nr/face-radial-indirect.md)。

服饰镜面输入通过双配置数值与真实模型检查。
范围见[服饰镜面验收](../acceptance/nr/cloth-specular-input.md)。
逐项顺序见[角色技术清单](2026-10-22-character-technique-closure.md)。

## 分项实现与证据

角色环境漫反射法线通过双配置分项检查。
身体皮肤候选及范围见[法线验收](../acceptance/nr/character-indirect-normal.md)。

原生方向场工具已接入三层及默认环境组合。
角色颜色组合与逐灯接收范围已通过公共机制检查。
主光遮挡通过[专项验收](../acceptance/nr/character-main-shadow.md)。
完整角色照明及分部位材质继续验收。

来源与运行待项见[组合验收](../acceptance/nr/irradiance-composition.md)。
三维上传及实际体积载荷见[资源验收](../acceptance/nr/volume-image-upload.md)。
三层资源与活动片元采样见[精度验收](../acceptance/nr/irradiance-clipmap-resource.md)。
公共材质按资源版本消费三层过渡和默认环境。

实际候选与生命周期见[接入验收](../acceptance/nr/irradiance-clipmap-scene.md)。
物理采样偏移与 NPR 位置边界通过双配置检查。
数值范围与正式待项见[查询验收](../acceptance/nr/irradiance-query.md)。

中央地面的索引、UV 别名流及完整材质参数已核对。
颜色上传、公共转换与主场景见[绑定验收](../acceptance/nr/floor-runtime-binding.md)。
继续核对独立纹理采样、反射生产通道及移动响应。
十二探针身份、反射生产链与对照镜头见[运行核验](../acceptance/nr/native-reflection-bindings.md)。

继续核对有序探针及真实时序比较。
公共对数映射及双配置 GPU 检查见[层级验收](../acceptance/nr/reflection-lod.md)。
原生数组、有序探针及连续时序继续实施。
有序体积、辐照亮度适配及生产消费见[探针验收](../acceptance/nr/reflection-probe-fields.md)。

双面镜像的法线、GI 及积分头发联合路径见[朝向验收](../acceptance/nr/surface-facing.md)。
帧内镜头报告及完整精度指纹见[来源验收](../acceptance/nr/capture-camera.md)。
原生数组、零符号约定及双配置硬件采样见[数组验收](../acceptance/nr/octahedral-environment.md)。

反射射线预计算通过双配置与动态遮挡回归。
短测及正式待项见[射线验收](../acceptance/nr/reflection-ray-math.md)。
本帧计算反射的主场景装配见[组合契约](../runtime/screen-reflection.md)。
半尺寸层级与源像素选择见[尺寸验收](../acceptance/nr/reflection-resolution.md)。

历史计算的冻结输入及公共资源见[时间过滤验收](../acceptance/nr/reflection-temporal.md)。
射线与时间核的公共组合见[时间组合验证](../acceptance/nr/reflection-temporal-composition.md)。
公共运动的选样及深度接收见[重投影验证](../acceptance/nr/reflection-reprojection.md)。
主场景历史、取消重试及 GPU 接收证据见[装配验收](../acceptance/nr/main-reflection-history.md)。

快照重复着色的成本依据见[逐事件诊断](../acceptance/nr/reflection-pass-profile.md)。
快照特化的双配置、图像及短测见[预算验证](../acceptance/nr/snapshot-shading-budget.md)。
原版四点搜索、足迹系数及精度待项见[射线来源](../acceptance/nr/native-ray-source.md)。
主宿主共享运动目标及双配置验证见[共享验收](../acceptance/nr/shared-motion.md)。

KK 动态边缘的双配置门槛见[运动边缘验证](../acceptance/nr/temporal-motion-edges.md)。
平台开关与运动附件的实际对应见[可见性验证](../acceptance/nr/motion-visible-surfaces.md)。
服饰及 KK 动态门槛见[轮廓显示验证](../acceptance/nr/temporal-display-edges.md)。
原版层级射线及动态时间历史继续验收。

中庭原生方向场已覆盖后方的已知照明缺口。
双配置检查及边界见[光照覆盖](../acceptance/nr/irradiance-coverage.md)。

环境镜面积分响应通过双配置数学及 GPU 检查。
原生运行待项见[镜面响应验收](../acceptance/nr/environment-specular.md)。

六面反射及独立投影原点通过双配置专项检查。
原生外观待项见[六面反射验收](../acceptance/nr/reflection-cube.md)。

眼睛径向材质已通过双配置数值及生命周期检查。
原生绑定待项见[眼睛验收](../acceptance/nr/eye-radial-surface.md)。
NPR 适配及捕获姿态完整文档见[眼睛输入](../acceptance/nr/eye-npr-inputs.md)。
原生采样器与硬件层级切换边界继续按 C9-E1 核对。

完整区域、层级边界、眼白及动态视线继续验收。

角色局部探针的镜面权重与诊断开关已通过检查。
双配置数值及实际脸部见[反射控制](../acceptance/nr/character-probe-controls.md)。

脸部高光正控制已完成解析及完整 GPU 套件核对。
门槛与原始记录见[正控制验收](../acceptance/nr/face-specular-control.md)。

无光照特效表面已通过双配置数值与生命周期检查。
原生物流转换及范围见[特效验收](../acceptance/nr/unlit-surface.md)。

二维发丝高光与表面响应已通过双配置数值检查。
范围及保留门禁见[高光验收](../acceptance/nr/projected-hair.md)。
实例外轮廓覆盖与真实脸部已完成分项验证。
原生双帧的材质帧实例及跨阶段调色板见[来源验收](../acceptance/nr/material-frame-instance-producer.md)。

身体骨盆原点及双配置验证见[径向原点验收](../acceptance/nr/body-rim-origin.md)。
蒙皮齐次位置及剩余严格图像门禁见[位置验收](../acceptance/nr/skinning-position-consistency.md)。
服饰二维高光的公共契约与实际画面边界见[服饰验收](../acceptance/nr/cloth-specular-ramp.md)。
服饰原生输入、组合照明及主候选见[接入验收](../acceptance/nr/cloth-native-integration.md)。

结果及兼容待项见[外轮廓验收](../acceptance/nr/outline-coverage.md)。

## 资源验证计划

原生可玩还原按 NR0 至 NR12 自主实施。
NR0 至 NR8 覆盖场景与角色原生渲染。
NR9 至 NR11 覆盖原生移动动作、WASD 与动画混合。
NR12 负责画面、操控、发布与实录联合验收。

执行任务见[可玩还原计划](2026-10-17-native-playable-restoration.md)。
公开资料、实景与抓帧按[证据执行计划](2026-10-21-native-render-evidence-execution.md)推进。
统一状态表登记全部 NR 阶段，局部证据随专项记录维护。
阶段状态与验收见[完整还原计划](2026-10-17-native-render-restoration.md)。

无遮挡参考与地标标定见[参考基准](../acceptance/nr/reference-baseline.md)。

后方模型、室内细节和抗锯齿见[专项计划](2026-10-21-atrium-completeness-antialiasing.md)。
605 个源实例组已进入正式与可玩工程。
来源、相机及展示见[中庭完整性验收](../acceptance/nr/atrium-completeness.md)。
固定遮罩来源覆盖与可见室内天空复核通过。

100 米深度门禁保持失败，完整原生外观继续验收。
主辅视口与 Player 的空间抗锯齿已有数值证据。
完整抗锯齿按[NR7-AA0 至 AA6](2026-10-22-spatial-antialiasing.md)实施。
运动历史、发丝、高光、描边和主场景画质保持待验。

角色与物流按[专项计划](2026-10-22-character-logistics-restoration.md)推进。
接入风险及公共契约见[架构审计](2026-10-22-render-capability-integration-audit.md)。
NR0-A1 固定有效入口，NR8-V2 管理候选晋升及跨入口验证。
NR6-C15 至 C18 覆盖材质、GPU 绑定、多层高光和距离描边。

公共距离投影与深度壳已进入[生产验证](../acceptance/nr/outline-material-integration.md)。
原生整区、动态晋升及完整抗锯齿分别保留门禁。
对象图集及原生过滤范围见[GPU 验证](../acceptance/nr/object-shadow-gpu.md)。
真实腿部保留七字节差异，完整角色外观继续待验。

逐材质局部灯下限及实际候选见[响应验证](../acceptance/nr/material-local-wrap.md)。
对象接收坐标已匹配原生可见量，见[分项验证](../acceptance/nr/object-shadow-receiver.md)。
对象隔离测得腿部为 `5/255`，与原生一致。
肩部与两个脸部点一致，完整皮肤颜色继续核对。

主场景消费与原生外观分别验收，正式阶段状态保持待验。
场景灯下的角色材质与高光已完成公共机制验证。
十发射器物流候选包含 91 个槽位。
四秒实际采样见[物流联合验收](../acceptance/nr/logistics-multiclip.md)。

角色与物流的四机位采集及实际输入回放通过。
1080p 工程视频与性能范围见[联合验证](../acceptance/nr/character-logistics-joint.md)。
公共乘法合成、节点透明度及渐隐描边已有分项证据。
辅助视口抗锯齿在匹配构建后通过八相位复验。

原生二维头发高光、眼睛通道及屏幕发影继续核验。
覆盖阴影的模板、预深度及原生遮挡具有独立任务。
原游戏帧的专项导出按[抓帧接手说明](2026-10-22-endfield-capture-handoff.md)推进。
正式 NR 阶段继续按完整门禁验收。

单角色、单场景、单动画与表情进入自主实施。
对象为现有莱万汀，EV0 完成。
原生主线已取得角色、动画、表情源与帝江号切片。
EV2 至 EV7 通用能力已实现，EV8 联合演示通过。

原生来源、历史画面及性能验收保持待项。
原生技术项目采用编队待机 01 与中性表情。
原生共享移动动作已完成目标骨架投影及隔离解码。
跑步的中庭短片段播放通过，循环与脚接触待验收。

走跑循环的完整解码时域已恢复，游戏时钟映射待核验。
证据见[原生移动动作](../acceptance/nr/native-locomotion.md)。
速度混合支持实体独立权重、共享步态相位及统一姿态消费。
公开双角色夹具覆盖三段动作与启停输入。

契约及实景范围见[运动混合证据](../acceptance/nr/locomotion-blending.md)。
帝江号联合采样、实例编辑与保存重开已验证。
场景表面通道烘焙与原生灯光读取已验证。
局部反射已接入独立纹理、盒投影与体积混合。

公共夹具八项通过，中庭反射候选四帧运行通过。
场景激活与视觉等价继续按 NR5 门禁验收。
公共信号层级、可信度消费和完整表面快照已通过分项检查。
接入顺序见[动态反射计划](2026-10-28-reflection-filter-integration.md)。

证据见[局部反射接入](2026-10-18-local-reflection-probe.md)。
逐灯镜面倍率已应用到局部灯与环境主光。

双后端测量及实景收据见[镜面倍率证据](../acceptance/nr/light-specular-response.md)。
方向网格的 GPU 求值与六组像素对照已验证。
原生组合按 NR4 的分支及实景门禁验收。
带光照展示见[帝江号展示计划](2026-10-17-dijiang-lighting-video.md)。

完整材质、衣摆效果与装载性能仍待验收。
范围见[资源验证实施计划](2026-10-16-endfield-resource-validation.md)。
本机样本与 REA 证据见[调查记录](../research/2026-10-16-endfield-resource-validation.md)。

## 当前交付记录

本轮范围见[路径追踪补齐与公开演示计划](2026-10-15-pathtracing-completion.md)。
PT0 至 PT7 已完成。
金属、法线、玻璃和灯光采样已接入通用渲染模块。
三个公开模型演示完成编辑保存、GUI 发布和移动运行。

双设备、收敛与画面结果见[交付记录](../acceptance/pt/2026-10-15.md)。

WC1 至 WC6 完成，范围见[世界参照与相机计划](2026-10-15-world-camera-rendering.md)。

## 界面改进实施

VD0 研究与设计完成，VD1 至 VD5 完成，VD6 为 Ready。
自动交付已验证，可进入集中人工复核。
启动入口与证据见[界面交付记录](../acceptance/vd/2026-10-14.md)。
范围、文件边界与验证安排见[界面改进计划](2026-10-14-ui-modernization.md)。

外观见[可交互视觉提案](../design/ui-modernization/preview.html)。
实施依次覆盖公共控件、项目中心、工作台和业务面板。
跨模块工作流与人工复核集中安排在 VD6。
本轮按 VD1 至 VD6 连续实施。

## 已有交付状态

UX-A 至 UX-D 完成，UX-E 为 Ready。
自动检查已完成，新版使用体验等待集中确认。
交付入口与结果见[本轮交付](../acceptance/ux/2026-10-14.md)。
范围与阶段记录见[交互优化计划](../editor-usability-audit.md)。

用户已反馈人工复核基本可用，交互体验需要优化。

ED0 至 ED7 的编辑器开发交付已完成。
P8 状态为 Ready，机器批次已收尾。
实体复核与历史现场条件按实际证据推进。
任务、文件边界和收尾条件见[本轮实施计划](2026-10-14-editor-generalization.md)。

当前阶段使用完成、Ready、开发中、待完成四种状态。
代号和状态见[统一状态表](current-stage-status.md)。
验收顺序与完成条件见[收尾流程](../acceptance/phase-closure.md)。
本轮流程修复见[执行计划](2026-10-12-workflow-repair.md)。

## 资源浏览器与日志面板

资源查找、空状态与日志工具栏使用公共控件。
日志模型负责级别、暂停快照与可见内容复制。
实施范围见[面板计划](2026-10-12-browser-console-ui.md)。
验证与剩余限制见[阶段记录](../acceptance/ui5/2026-10-12.md)。

层级、属性和场景视口迁移完成。
安装入口及集中自动检查已交付。
结果与剩余复核见[本轮交付](../acceptance/ui8/2026-10-12.md)。
实施和集中交付见[剩余计划](2026-10-12-remaining-ui.md)。

人工输入和既有交付待项按集中复核条件推进。

## 项目中心与独立游戏交付

本轮授权范围是 ImGui 产品化与独立制作发布。
四阶段实现及自动工作流检查已执行。
范围见[产品化状态](../ui-productization-status.md)。
证据与未通过项见[交付验证](../acceptance/ui4/2026-10-12.md)。

工作区面板迁移完成，后续进行集中人工复核。
场景视口的布局与交互已完成本批检查。
实体输入和历史超时原因按集中复核记录。
实施依据见[产品化计划](2026-10-12-imgui-productization.md)。

本页回答：哪些工作已完成，下一批工作按什么顺序推进？能力与证据范围见[项目现状](../project-status.md)，执行规则见[协作与验收](../development-workflow.md)。

## 当前阶段

材质画布、音频修复、格式扩展和二维内容已完成可推进部分。
项目中心与 GUI 发布按本页的产品化交付范围使用。
正式性能和持续运行结果见本轮 P8 收尾记录。
人工体验与历史归因按集中复核条件推进。

## 已完成的工作

| 工作组 | 实现结果 | 详细来源 |
| --- | --- | --- |
| 引擎基础与三维交付 | 公共运行契约、编辑操作、探索项目和独立发布 | [引擎实施计划](2026-10-06-engine-evolution.md) |
| 编辑器制作体验 | 工作区、选择、变换、内容制作与双工作流自动制作 | [产品化计划](2026-10-09-editor-productization.md)、[P6 验收](../acceptance/p6/2026-10-12.md) |
| 第一批格式交换 | 输入注册、导出、音频与纹理交换、移动发布 | [格式计划](2026-10-11-asset-format-expansion.md)、[P8 验收](../acceptance/p8/2026-10-12.md) |
| 内容扩展与修复 | 材质图、二维文档、GIF/PSD/SVG/USD、音频及路径修复 | [内容扩展计划](2026-10-12-content-expansion.md)、[P9 验收](../acceptance/p9/2026-10-12.md) |
| 通用编辑器制作 | 基础创建、层级、组件、外部拖放、设置与特性隔离 | [ED7 集成记录](../acceptance/ed7/2026-10-14.md) |

“实现结果”描述交付物，完整验收状态按对应来源读取。最近一轮提交和测试范围集中列于[项目现状](../project-status.md)。

## 接下来的顺序

终末地专项按[可玩还原计划](2026-10-17-native-playable-restoration.md)执行。
资产读取沿用[原生资产获取计划](2026-10-17-endfield-native-assets.md)。
目标为原生角色、动作、表情与场景的引擎闭环。
读取路径按实际证据选择，引擎扩展服务于资产兼容。

分层资源读取及索引验证见[读取证据](../acceptance/nr/layered-content.md)。
139 个实例的最高细节转换见[几何证据](../acceptance/nr/source-geometry-detail.md)。
下表维护引擎集中验收待项。

| 顺序 | 工作 | 当前状态 | 交付与继续条件 |
| --- | --- | --- | --- |
| 1 | P8.1 候选与回归 | 完成 | 双配置回归与双设备移动发布通过，实体输入另行复核 |
| 2 | P8.2 历史调查 | Ready | 七份原件已核对，三项归因需复现时的现场记录 |
| 3 | P8.3 持续运行 | 完成 | 两种三十分钟正式长跑通过，短探针失败单独保留 |
| 4 | P8.4 性能 | Ready | 本轮十五轮有八轮通过，七轮热状态失败，按来源复核 |
| 5 | P8.5 人工复核 | Ready | 操作单与候选已准备，实体操作和听感待执行 |
| 6 | P8.6 汇总 | Ready | 机器证据与交付清单已整理，全部条件通过后完成 |

开发期间执行编译、受影响短测试和必要修复。
正式验收待项由 P8 持有，开发按交付物依赖推进。
原条件、失败记录和未执行结果按实际证据保存。
其他候选功能见[后续开发路线](2026-10-11-next-development.md)。

## 候选功能

完整二维可视编辑器、通用着色程序图、更完整的 USD 交换属于候选。进入实施时需要具体使用场景、支持边界和验收范围。Android 保持延期（`Deferred`），桌面制作和发布是当前平台范围。

## 状态维护

当前能力和限制维护于[项目现状](../project-status.md)。本页维护工作顺序。专项计划维护接口与任务。验收页维护证据。

阶段完成时同步这些入口和相关清单，并按授权范围创建独立提交。

历史计划与候选结果见[整理前快照](../history/2026-10-12-planning/azure-engine-plan.md)。历史测试数字与运行结果按原来源解释，当前判断使用对应版本的证据。

P8.1 至 P8.6 的执行结果见[集中收尾记录](../acceptance/p8/2026-10-14.md)。

WC1 至 WC6 完成，证据见世界参照与相机实施记录。
本轮覆盖双设备路径追踪、语言切换和独立发布。
