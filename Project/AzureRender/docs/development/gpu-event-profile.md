# GPU 事件成本分析

逐事件分析用于定位计算及绘制的主要成本。
入口为 `tools/native_render/renderdoc_profile.py`。
该模块在兼容的 RenderDoc Python 环境中运行。
调用者提供原始捕获路径和独立输出目录。
运行时性能仍由 Player 的 GPU 时间报告验收。

## 操作顺序

1. 固定候选、分辨率、机位及渲染配置。
2. 用 RenderDoc 捕获指定帧，并等待目标程序退出。
3. 在同一设备上调用 `profile_capture`。
4. 核对 `profile.json` 的来源指纹及事件汇总。
5. 根据高耗时事件定位所属着色程序和生产步骤。

以下代码运行于已提供 `renderdoc` 模块的解释器。
仓库的 `tools/` 目录需位于 Python 模块搜索路径。
路径参数由当前任务的 D 盘目录提供。

```python
import renderdoc
from native_render.renderdoc_profile import profile_capture

result = profile_capture(renderdoc, capture_path, output_directory)
```

捕获、重放和其他 GPU 验收串行运行。
调用结束后释放重放控制器和捕获句柄。
必要的计数摘要进入正式证据目录。
可重建缓存按[产物管理](artifact-retention.md)清理。

## 数据含义

`measurements` 保留事件号、动作名及秒单位时长。
计算与绘制按活动着色程序身份分组。
默认查询耗时最高的 64 个事件的管线。
`shader_event_limit` 接受一至 512。
其余事件保留时长，并标记为 `uninspected`。
`groups` 按毫秒成本降序排列。
`sumEventMs` 汇总计数器返回的事件时长。
事件时长之和与实际帧延迟分别记录。

`source` 保存原始捕获的路径、字节数和 SHA-256。
`shaders` 记录阶段、常量块尺寸及程序字节指纹。
可用源文件名随反射信息一同记录。
重复事件、空计数、负值和非有限时长会触发拒绝。
重放干预、驱动缓存及捕获开销属于测量边界。
完整性能门槛沿项目既有运行时预算判断。
