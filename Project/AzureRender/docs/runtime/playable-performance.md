# 可玩关卡性能与加载

> 文档类型：概念与操作指南
> 状态：生效
> 更新日期：2026-10-07
> 适用范围：Windows Player 与编辑器
> 源码入口：`LevelSession`、`AssetDatabase`、`CharacterSceneRenderer`
> 关联测试：`AssetLevel`、`NullRhiPass`、正式负载与循环切关

## 所有权与提交

主线程持有活动数据库、World 与渲染器。后台任务使用数据库副本，扫描并解析候选关卡。候选通过校验后，主线程分帧准备 GPU 资源。

请求使用递增版本和原子取消标记。新请求使旧候选失效，扫描按文件及每 MiB 检查取消。任务退出时等待后台工作结束。

预加载缓存最多持有两个候选关卡。解析后的模型通过只读共享指针持有。资产指纹变化使对应候选失效。

GPU 准备按 4 ms 时间片调度。网格、材质纹理、描述符和管线分别作为上传任务。单个上传任务完成后检查时间预算。

活动关卡持续运行，直到候选全部就绪。关卡与组件在帧边界执行事务替换。准备失败保留有效 World 与数据库，并报告错误。

相同资源指纹的关卡复用活动渲染器。节点身份、资源映射、灯光与播放状态随关卡刷新。替换渲染器时等待在途 GPU 工作完成。

## 动画与资源容量

资源路径经规范化后映射到唯一网格。多个资源身份可引用同一网格与纹理。实体各自持有播放时钟、混合状态和姿态。

关节包围盒在加载时建立。每帧根据关节矩阵与 Morph 权重求保守范围。透明排序为同一实例缓存已变形顶点。

实例关节与蒙皮缓冲按实际容量分配。在途帧数量为二，扩容仅修改已完成栅栏的帧资源。完整动画更新由正式负载预算验证。

## 诊断字段

GPU 报告记录逐帧 CPU 工作、等待和固定步耗时。工作时间包含编辑器、玩法、动画和录制。栅栏、获取图像、提交和呈现耗时归入等待。

内存报告区分累计分配与活动字节。专用显存峰值统计 VMA 的设备本地内存块。进程工作集由 Windows 进程查询采集。

CPU 与 GPU 百分位窗口最多保存 8192 帧。物理样本随对应帧淘汰，资源记录也有容量上限。GPU CSV 持续写入全部有效帧。

资源遥测通过类型化环形记录保留 8192 条样本。
宿主主线程维护记录，编辑器与 Player 共用规则。
JSON 字段在报告写出时逐条序列化。
采样频率、提交记录和字段语义由现行契约定义。

`sampleOffsetFrame` 记录 CPU 窗口的首帧偏移。`resourceFrames` 记录关卡提交与周期资源状态。报告还包含网格、三角形、透明面与蒙皮容量。

## 测量入口

在 Release 构建后执行正式负载测试。每轮先预热 300 帧，再采样 1800 帧。公共标准、编辑器和压力负载分别运行三轮。

```powershell
python tools/run_playable_performance.py `
  --player build/ninja-msvc-release/AzurePlayer.exe `
  --editor build/ninja-msvc-release/AzureRender.exe `
  --output build/performance/playable
```

私有主角使用可选参数 `--character <GLB>`。输出目录必须使用新的路径。每轮开始时 GPU 温度至多为 55°C。

```powershell
python tools/test_level_cycles.py `
  --player build/ninja-msvc-release/AzurePlayer.exe `
  --output build/performance/cycles
```

循环验收包含取消候选、预加载与资源身份变化。十轮各执行两次切关和两次重开。提交额外耗时 P99 至多为 8 ms。

具体帧预算见[第三人称开发计划](../plans/third-person-playable-plan.md)。测量保存场景哈希、源码指纹、设备和质量配置。结果按每轮预算判定。

完整独立包与真实长跑入口见[Windows 游戏发布](game-publishing.md)。长跑连续写入全部 GPU 帧，并记录每分钟预算。窗口恢复后，按固定分辨率比较稳定驻留和工作集。
