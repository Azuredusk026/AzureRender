# 黑洞性能预算

> 文档类型：运行说明
> 状态：生效
> 更新日期：2026-10-03

## 算法与采样

电影档使用四条射线、1800 步上限和厚体积吸积盘。盘采样先检查径向范围和保守高度上界，再计算噪声与辐射。高度上界包含浮点安全裕量。

性能验收采用本机 Release、1280×720、正面相机和固定模拟步长。每轮 300 帧，三轮 GPU 平均时间的中位数须低于 20 ms。

## 验收命令

保留基线安装树后，在工程目录执行。每个输出目录须为空：

```powershell
python tools/test_blackhole_optimization.py
python tools/run_blackhole_optimization.py --executable build/ninja-msvc-release/AzureRender.exe --output-dir build/blackhole-optimization/screen --mode screen
python tools/run_blackhole_optimization.py --executable build/ninja-msvc-release/AzureRender.exe --baseline-executable build/blackhole-optimization/baseline/bin/AzureRender.exe --output-dir build/blackhole-optimization/images --mode images
python tools/run_blackhole_optimization.py --executable build/ninja-msvc-release/AzureRender.exe --output-dir build/blackhole-optimization/performance --mode performance
```

## 计时范围

整帧 GPU 查询覆盖阴影清理、主场景和宿主后处理。黑洞主场景包括追踪、Bloom、时间累积和合成。GPU 查询不包含 CPU 工作、窗口呈现与帧槽等待。

CPU 录制计时包含图准备、录制任务等待和图执行。帧槽 fence 约束在途资源复用。截图读取等待对应帧完成，资源重载和交换链重建等待设备完成工作。

呈现优先使用邮箱模式（MAILBOX），设备能力不足时使用先进先出模式（FIFO）。实际交互帧率由 GPU、CPU、呈现策略和显示设备共同决定。

## 证据

版本、设备、图像对照与正式性能结果见 [黑洞优化验收](../acceptance/blackhole/2026-10-03.md)。
