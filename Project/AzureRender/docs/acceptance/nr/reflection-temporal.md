# 反射时间过滤验收

日期：2026-10-29，阶段为 NR5-R8-K。
范围为官方 3534 帧的历史计算及公共计算资源。
运行契约见[反射时间过滤](../../runtime/reflection-temporal.md)。

## 原版重建

事件 8732 更新历史可信度，8741 更新颜色历史。
两项计算均使用三乘三邻域及 1.25 方差尺度。
历史接收比例与 0.97 系数共同控制合成量。
运动输入沿已有工具解码为公共方向的 UV 位移。

重建可信度的 921,600 字节与原版完全一致。
重建颜色的 3,686,400 字节与原版完全一致。
方差边界保留显式乘加及舍入次序。
零输出替换程序分别改变 547,865 个可信度字节。
颜色负例改变 3,680,418 字节，确认程序参与实际重放。

## 公共资源

计算步骤借用全部输入和输出图像。
参数、描述符及管线按逐帧槽位持有。
私有参数为 32 字节，描述符绑定共九项。
历史图像及提交状态由宿主统一持有。

Debug 和 Release 的组件检查通过。
每配置完成二十四份合成输出及四份真实输入输出。
合成检查覆盖历史开关、屏外拒绝和运动方向。
零接收比例、零半径裁剪及槽位复用分别检查。
零半径夹具采用方差尺度零，真实载荷采用原版 1.25。
图像和缓冲在有效运行后释放归零。

## 数值结果

两配置的四份真实输入输出分别逐字节一致。
紧凑格式可信度与原版逐字节一致。
紧凑颜色有十一个像素相差一个原版格式量化单位。
最大颜色误差为一个单位，预算为 1.01。
浮点输出同样按原版格式的量化单位核对。
其最大可信度误差为 0.560006 字节，预算为 1.01。
零权重槽位的浮点颜色与当前信号逐字节一致。

全部旧着色模块的指纹与半尺寸生产候选核对。
完整场景的视觉证据沿[半尺寸生产](reflection-resolution.md)引用。
主场景共享运动、连续历史及正式稳定性继续验收。

## 迁移复测

夹具保存六项已解码输入及输入指纹。
参考结果复用已归档的原版颜色和可信度基础层。
压缩包及重建程序见[夹具收据](evidence/2026-10-29/reflection-temporal-fixture/receipt.json)。
输出目录使用 D 盘工作区中新建的运行目录。

```powershell
python -m zipfile -e docs/acceptance/nr/evidence/2026-10-29/reflection-temporal-fixture/fixture.zip build/runs/<运行目录>/fixture
build/ninja-msvc-debug/AzureReflectionTemporalGpuTests.exe build/ninja-msvc-debug/shaders/reflection_temporal_packed.comp.spv build/ninja-msvc-debug/shaders/reflection_temporal_float.comp.spv build/ninja-msvc-debug/shaders/reflection_signal_pyramid_packed.comp.spv build/ninja-msvc-debug/shaders/reflection_signal_pyramid_float.comp.spv build/runs/<运行目录>/fixture build/runs/<运行目录>/output
python tools/check_reflection_temporal_fixture.py --fixture build/runs/<运行目录>/fixture --actual build/runs/<运行目录>/output --output build/runs/<运行目录>/comparison.json
```

检查程序核对输入摘要、完整尺寸及固定误差预算。
源码、构建、程序和运行指纹见[机器记录](evidence/2026-10-29/reflection-temporal.json)。
