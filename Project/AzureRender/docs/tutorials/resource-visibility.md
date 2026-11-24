# 可见性对照教程

> 文档类型：操作教程
> 适用平台：Windows

在工程根目录准备 Release 构建。
使用已配置的 MSVC、Vulkan SDK 和 vcpkg。
独立探针要求本机安装 Vulkan 验证层。

```powershell
./tools/msvc_env.bat cmake --build build/ninja-msvc-release
python tools/run_visibility_scale_benchmark.py --output build/evolution/r9/scale-manual
```

输出目录必须是新的目录。
对照包含 CPU、默认、原型与固定描述符原型。
每组保存图像、时间、能力与原始日志。
汇总只有在完整矩阵和预算通过后标记成功。

单独预览原型使用以下命令。
运行时保留默认材质、阴影和透明设置。

```powershell
./build/ninja-msvc-release/AzureRender.exe --asset assets_public/test_model.gltf --instances 100 --visibility-prototype
```

使用 `--disable-bindless` 选择固定描述符。
使用 `--disable-gpu-culling` 选择 CPU 提交。
设备能力不足时，宿主采用可用的正式路径。
契约和预算见[资源访问与可见性](../runtime/resource-visibility.md)。
