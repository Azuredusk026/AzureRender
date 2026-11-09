# 论文三路径基准

本目录属于论文独立实验目标。
源码基点为 `3f9c745`。
论文材料位于 `D:/Assigment/2609/FYP2`。
实施与验收见 [执行计划](docs/execution.md)。

## 构建入口

本目录的 CMake 工程独立配置。
它使用 C++17 与既有 Vulkan SDK。
构建输出保存到已登记的 D 卷受管任务。
编译和 GPU 操作通过统一租约入口执行。

```powershell
cmake -S experiments/thesis -B <受管任务>/build -G Ninja -DCMAKE_BUILD_TYPE=Release
cmake --build <受管任务>/build --parallel 2
ctest --test-dir <受管任务>/build --output-on-failure
```

基准程序为 `AzureThesisBenchmark`。
`--path` 接受 `traditional`、`subpasses` 及 `drlr`。
`--shaders` 指定构建生成的着色器目录。
`--output` 指定受管任务内的结果目录。

```powershell
AzureThesisBenchmark --path drlr --gpu NVIDIA --width 1920 --height 1080 --complexity low --shaders <构建目录>/shaders --output <任务目录>/pilot --warmup 3 --frames 12 --validation
```

独立捕获可配置 `THESIS_RENDERDOC_DIR`。
运行时用 `--renderdoc` 指定官方动态库。
`--capture` 指定任务目录内的捕获文件前缀。
捕获进程单独设置 RenderDoc 层路径及启用环境。

程序输出逐帧时间、实际调用数及最终图像。
当前宿主采用离屏渲染及逐帧完成等待。
交换链与交互演示仍属于 O1 的待接入项。
正式数据需补充环境资格及冻结清单。

## 证据范围

设备查询证明功能与资源能力。
实际路径还需命令、着色器及运行检查。
正式矩阵在设备资格与试运行后冻结。
现有第一章、第四章及预备数据按原状态引用。
