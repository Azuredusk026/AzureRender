# 并行录制与 GPU 提交

> 文档类型：运行时说明
> 状态：生效
> 更新日期：2026-10-03
> 适用范围：Windows Vulkan Character 渲染路径
> 源码入口：RecordingWorkerPool.hpp、WorkerCommandPools.hpp、GpuCullingResources.hpp
> 关联测试：AzureRender.R3Comparison、AzureRender.R3TransparentComparison、AzureRender.FrameTaskScheduler、AzureRender.NullRhiPass

## 职责与使用场景

Character 蒙皮、剔除、阴影和主场景分别录制命令缓冲。主线程按帧图编译顺序执行，GPU 依赖不因 CPU 并行而改变。

## 数据与所有权

RenderFrameSnapshot 持有相机输入与渲染设置副本。renderer 在更新阶段复制设置，实例和资源在录制任务结束前保持有效。GPU 资源直到对应帧 fence 完成后才能重用。

每个帧槽和工作线程独占命令池。secondary 缓冲按任务峰值扩容，重置后复用。剔除资源持有包围盒、源命令和间接输出缓冲。

SceneInstanceSnapshot 持有只读实例、可见索引、批次、设置、编辑变换和本帧缓冲句柄。回调持有共享快照，下一帧准备数组不会改变已发布数据。注册前校验实例映射与批次范围。

管线、描述符布局和网格资产属于加载期资源，录制期间不得卸载或重建。快照借用的GPU句柄由帧资源所有者保持到fence完成，快照本身不销毁资源。

阴影和主场景回调共享只读上下文，显式传入独占 recorder 和统计槽。录制阶段直接读取已发布数据。

实例包围盒覆盖本帧 Morph 与蒙皮顶点，形变顺序与着色器一致。每个网格每帧计算一次局部范围，各实例再应用世界变换，CPU 和 GPU 剔除共享范围。

## 生命周期与时序

等待帧 fence 后重置命令池并准备帧数据。全部工作任务结束后才执行图。关闭时先等待 GPU，再释放命令池和渲染资源。

## 接口契约

addGraphicsPass 描述 render pass 边界和内部绘制。工作线程只录制内部命令，主缓冲负责开始和结束。Compute 输出使用 IndirectBuffer 声明读取，生成 DRAW_INDIRECT 屏障。

## 线程与同步

RecordingWorkerPool 使用四个常驻线程，主线程也参与录制。五个录制线程各自使用稳定编号和独立命令池。任务读取本帧状态，写入独立命令缓冲。主场景计数在任务完成后合并。

主线程保留首项任务，工作线程领取其余任务。任务按帧图编译顺序和片段序号入队。主缓冲按相同顺序执行命令。

任务批次持有只读任务列表和独立异常槽。线程通过原子索引领取任务，最后完成的任务通知主线程。延迟唤醒的线程保留批次所有权，批次存储保持到全部引用释放。

透明实例按快照中的可见索引顺序划分，最多四个连续片段。每八个可见实例增加一个片段，各片段独立统计提交次数。主缓冲按原始片段顺序执行，保持透明绘制顺序。

全部网格蒙皮由一个 Compute Pass 描述，每个网格分别调度。该 Pass 声明全部输入、输出和共享关节缓冲。录制使用一个 secondary 命令缓冲。

调度仅允许创建线程调用。主线程任务和工作线程任务内的嵌套调度均明确报错。任务异常在全部任务完成后传播，后续调度可以继续运行。关闭时调用方须保证调度已结束。

ISceneRenderer 的更新、卸载和交换链重建不得与图录制或执行重叠。宿主先准备帧数据，再等待全部工作任务，之后才提交和进入下一次更新。默认注册回调按值持有上下文，借用资源的所有者必须保持有效。

透明索引排序在更新阶段上传，每个实例使用独立区间。帧图声明 HOST 写入到 INDEX_READ 的依赖。录制阶段读取已经准备的批次和索引，GPU 输入不再由工作线程修改。

## 序列化与兼容

不引入场景格式变化。计时 JSON 增加 recordingMilliseconds 与 workerRecordedPasses，报告实际 API 调用次数。

instances、visibleInstances 和 visibleRatio 记录 CPU 参考列表的累计对象数与可见比例。它们用于性能场景说明，不代表 GPU 输出回读验证。

indirectDrawCalls 记录实际间接 API 调用次数。四路径测试要求 CPU 用例为零，GPU 不透明用例非零，避免功能意外回退后仍报告一致性通过。

## 平台行为

缺少 Compute 或 drawIndirectFirstInstance 时使用 CPU 可见性和直接绘制。支持 multiDrawIndirect 时批量提交，按 maxDrawIndirectCount 拆分。Android 保持 Deferred。

## 使用示例

```powershell
python tools/run_r3_comparison.py --executable build/msvc-debug/AzureRender.exe --output-dir build/msvc-debug/r3-comparison --frames 10 --instances 64
```

四条路径逐帧哈希应一致。使用 --disable-parallel-recording 切换主线程录制，--disable-gpu-culling 切换 CPU 可见性。

对照还运行强制单命令间接路径。--disable-multi-draw-indirect 验证无批量间接能力时的行为，GPU 剔除仍启用。

```powershell
python tools/run_r3_performance.py --executable build/msvc-release/AzureRender.exe --output-dir build/perf/R3 --generate-resources 32 --frames 300 --repeats 3 --check-recording-budget
```

工具生成多资源公共场景，顺序采样四组合并保存中位数。小场景的调度开销和多资源场景的录制收益分别报告。

## 诊断与排错

workerRecordedPasses 为零时确认开关和场景。Validation 报错时检查继承的 render pass、framebuffer、帧 fence 和资源状态。性能比较独占运行，避免构建或其他 GPU 测试干扰。

## 验收与证据

Debug 64 实例十帧与 Release 256 实例三十帧的四路径哈希一致。证据见审计修复跟踪。R3 阶段为 Complete，性能采样与自动判定按 `r3-budget-v2` 达标。功能回归和发布门禁通过。

当前结果见 [R3 阶段验收](../acceptance/r3/2026-10-03.md)。

透明变体由公共资产临时生成，材质设为 Blend 并使用十六个实例。Debug 十帧四路径哈希一致，Validation 无错误。夹具和日志保存在测试输出目录。

## 参考来源

接口按 Vulkan secondary command buffer、multiDrawIndirect 和 drawIndirectFirstInstance 契约实现。剔除沿用项目六平面 AABB 正顶点规则。
