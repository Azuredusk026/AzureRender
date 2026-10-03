# Windows 渲染核心验收

> 文档类型：运行说明
> 状态：生效
> 更新日期：2026-10-03

## 场景与采样

公开压力场景为 `assets_public/scenes/r4_stress.azscene`。场景包含 32 个独立公共网格资源、32 个可见对象及 16 个点光源。角色采用四种录制与剔除组合，黑洞覆盖三个质量档位。复杂场景四路径的两帧截图须逐帧哈希一致。

性能采样使用 1280×720 和固定模拟步长。角色每组合 300 帧，黑洞每档 150 帧。每组保存 GPU 逐帧 CSV、统计 JSON 和执行日志。

## 生命周期验收

使用 Debug 执行角色和黑洞两场景长跑，每场景至少 120 秒。工具只操作自身子进程窗口。每场景执行六轮尺寸变化和最小化恢复，触发截图并正常关闭。窗口初始尺寸为 1280×720，重建日志记录实际客户端尺寸。

交换链重建与黑洞历史帧重置记录在运行日志。关闭后分配器存活缓冲和图像数须为零。截图须成功解码并包含非恒定像素。

## 执行命令

在工程目录顺序执行，输出目录须为空：

```powershell
python tools/test_r4_acceptance.py
python tools/run_r4_acceptance.py --executable build/ninja-msvc-release/AzureRender.exe --output-dir build/r4-performance --mode performance
python tools/run_r4_acceptance.py --executable build/ninja-msvc-debug/AzureRender.exe --output-dir build/r4-lifecycle --mode lifecycle
ctest --test-dir build/ninja-msvc-debug --output-on-failure
cmake -DBUILD_DIR=build/ninja-msvc-release -DCONFIG=Release -P tools/run_release_gate.cmake
```

## 报告口径

报告记录本机设备、驱动、工具链和源码版本。整帧 GPU 耗时与 CPU 录制分别列示，性能适用范围依据实际工作负载说明。长跑结果覆盖观察时段内的稳定性。

完整证据与阶段结论见 [R4 阶段验收](../acceptance/r4/2026-10-03.md)。
