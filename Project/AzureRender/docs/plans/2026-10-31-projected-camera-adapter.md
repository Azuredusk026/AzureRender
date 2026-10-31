# 投影射线相机适配实施计划

执行方式：当前会话自主实施。
目标：使投影射线消费引擎相机及线性 UV 运动。
前置提交为 `5072f39`，归属 NR5-R8-R5 的输入准备。
依据见[反射接入计划](2026-10-28-reflection-filter-integration.md)。

## 公共契约

相机适配归属 RenderCore。
输入复用 `ReflectionTraceView` 的投影、正交基和位置。
当前与前帧分别提供实际采样投影。
输出为 `ProjectedReflectionView` 的具名变换。

`projectedReflectionView(current, previous)` 构造适配视图。
`previous` 为空时使用当前视图的变换。
输入使用透视投影及零近一远的设备深度。
适配结果采用反向设备深度及 Vulkan 上方像素原点。

`ProjectedReflectionConvention` 声明 Y 方向约定。
`MirroredViewY` 延续公共内核的默认数学约定。
`VulkanUpperLeft` 用于引擎正高度视口。
约定进入视图参数，来源编号保持在测试层。

`ProjectedReflectionMotionEncoding` 声明运动编码。
`FourthRoot` 表示以 0.5 为中心的带符号四次方根。
`LinearUv` 表示当前减前帧的线性 UV 位移。
两种编码共用原有 480 字节参数布局。

## 文件职责

| 文件 | 职责 |
| --- | --- |
| `src/render/ProjectedReflectionCamera.hpp` | 相机适配声明 |
| `src/render/ProjectedReflectionCamera.cpp` | 变换构造及透视检查 |
| `src/render/ProjectedReflectionRay.hpp` | 坐标及运动编码枚举 |
| `src/render/ProjectedReflectionRay.cpp` | 枚举校验及打包 |
| `shaders/projected_reflection_trace.hlsl` | 两类方向与运动解码 |
| `tests/ProjectedReflectionCameraTests.cpp` | 深度和世界位置往返 |
| `tests/ProjectedReflectionCameraGpuTests.cpp` | 独立镜面的实际命中 |
| `tests/support/ComputeStorage.hpp` | 共享存储图像夹具 |
| `tools/test_projected_camera_gpu.py` | 正向、静态历史和失败控制编排 |
| `CMakeLists.txt` | 适配源码、GPU 夹具及编译依赖 |

## 检查重点

- 非有限相机和非法枚举在资源使用前拒绝。
- 正交相机按明确的输入类型诊断。
- 视图平移、旋转、投影偏移及前帧分别验证。
- 来源默认方向与编码继续满足完整字节门槛。
- 独立镜面同时验证方向、颜色、可信度及运动衰减。

## 相机与参数

- [x] 编写投影、深度往返及编码声明的红测。
- [x] 运行并保存缺失适配接口的预期失败。
- [x] 构造反向投影、相对世界投影及逆变换。
- [x] 增加枚举校验及帧参数打包。
- [x] 验证边界、非法输入及前帧变换。

## 独立 GPU 镜面

- [x] 将存储图像夹具提取为测试共享资源。
- [x] 构造有色墙面和地面镜面的独立输入。
- [x] 先保存方向或编码不匹配的实际 GPU 红测。
- [x] 在公共程序中接入所声明的方向及运动规则。
- [x] 验证镜面区域的实际命中及静态运动衰减。
- [x] 保留错误方向及错误运动编码的失败控制。
- [x] 核对边缘宽度命名、公式及完整图像响应。

## 回归与提交

- [x] 双配置构建期间冻结源码，并行度为二。
- [x] 正逆瓦片的三份完整来源输出继续逐字节一致。
- [x] 公共射线、镜面、深度及历史过滤通过受影响检查。
- [x] 核对验证层、同步、释放及程序指纹。
- [x] 同步文档、构建记录和独立阶段提交。

后续输入准备按表面快照的深度和法线契约实施。
主场景装配继续持有成功提交及取消重试的身份。
正式画面、连续序列和持续性能保持既有门禁。
