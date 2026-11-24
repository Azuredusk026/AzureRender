# 构建范围与来源核验

## 目标选择

构建在已配置的 MSVC、Vulkan 和托管开发环境中执行。
命令从 `Project/AzureRender` 目录启动。
日常短检选择运行目标和本轮相关的测试目标。
集中交付执行全部配置目标及正式回归。

| 参数 | 构建范围 | 收据范围 |
| --- | --- | --- |
| `build` | 全部配置目标 | 运行程序、着色器及已配置模块 |
| `build --runtime-only` | 运行程序与契约目标 | 对应运行产物及模块 |
| `build --test-target NAME` | 运行目标及指定可执行测试 | 运行产物及所选测试程序 |
| `verify` | 读取已有收据 | 源码、产物和所选测试身份 |

`--test-target` 可重复使用，目标名称按首次出现去重。
运行目标包含编辑器、Player、资源工具和材质验证器。
Windows 同时构建独立游戏启动器。
启用的托管和着色模块按配置共同构建及核验。

## 日常命令

批处理入口使用 Python 的绝对路径。
以下命令构建运行产品和相机测试程序。

```powershell
$taskRoot = (Get-Location).Path
$taskPython = Join-Path $taskRoot "build/reinstall-environment/venv/Scripts/python.exe"
$taskBuilder = Join-Path $taskRoot "tools/build_provenance.py"
.\tools\msvc_env.bat $taskPython -X utf8 $taskBuilder build --source $taskRoot --build-dir "$taskRoot/build/ninja-msvc-release" --config Release --test-target AzureCameraSystemTests
```

对应测试通过 CTest 的实际注册名称选择。
构建收据只声明编译及产物核验结果。
测试结果由所属 CTest 或专项检查另行记录。

```powershell
.\tools\msvc_env.bat ctest --test-dir build/ninja-msvc-release -R "^AzureRuntime.CameraSystem$" --output-on-failure
```

Debug 使用对应构建目录和配置参数。
集中构建调用 `build`，并选择完整回归范围。

## 收据与失败

`build-provenance.json` 保存源码和产物 SHA-256。
`buildScope` 列出完整、运行或运行加测试的构建范围。
所选测试程序具有独立的产物路径映射。
源码变化、产物缺失及已记录产物篡改均使验证失败。

引擎契约同时核对文件清单与对应字节。
着色程序和已启用模块的输出进入产物摘要。
文件摘要通过固定大小的数据块计算。
失败构建和途中源码变化保留既有收据。

完整画质、性能、设备及长跑使用所属正式门禁。
本轮验证范围见[构建流程验证](../acceptance/recovery/scoped-build-provenance.md)。
