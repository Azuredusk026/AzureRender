# 使用托管脚本制作独立项目

> 文档类型：教程
> 适用范围：Windows x64 与 G11 可选原型

## 准备构建

安装 .NET SDK 9、MSVC 和引擎工具链。
CoreCLR 项目运行时使用 .NET 9.0.11。
NativeAOT 项目运行时使用原生模块。
以下命令在工程目录执行。

```powershell
$env:VCPKG_ROOT = "C:/path/to/vcpkg"
./tools/msvc_env.bat cmake --preset msvc-release -DAZURE_ENABLE_MANAGED_SCRIPT_PROTOTYPE=ON
./tools/msvc_env.bat python tools/build_provenance.py build --build-dir build/ninja-msvc-release --config Release
```

构建生成两个后端及公共脚本 API。
结果位于 `build/ninja-msvc-release/managed/`。
其中的 `manifest.json` 记录哈希和构建测量。
默认构建使用 Lua 服务。

## 创建场景标注项目

选择 CoreCLR 后端并创建空目录项目。
示例类型通过公共组件接口修改物体位置。
项目中的清单保存类型与程序集引用。

```powershell
python tools/create_managed_project.py --player build/ninja-msvc-release/AzurePlayer.exe --artifacts build/ninja-msvc-release/managed --backend coreclr --workflow annotation --output "D:/Games/Managed Annotation"
./build/ninja-msvc-release/AzurePlayer.exe --project "D:/Games/Managed Annotation/project.azureproject" --check-project
./build/ninja-msvc-release/AzureRender.exe --editor-project "D:/Games/Managed Annotation/project.azureproject"
```

点击 Play 后，脚本将位置设为 `[4, 5, 6]`。
停止预览后，编辑文档保持制作时的状态。
运行错误通过脚本诊断面板查询。

## 创建角色控制项目

使用 `--workflow controller` 创建控制示例。
该项目装配共享角色、物理、动画与相机机制。
脚本通过动作名读取输入并提交运动请求。
WASD、空格与 Shift 分别控制移动、跳跃和冲刺。

```powershell
python tools/create_managed_project.py --player build/ninja-msvc-release/AzurePlayer.exe --artifacts build/ninja-msvc-release/managed --backend nativeaot --workflow controller --output "D:/Games/Managed Controller"
./build/ninja-msvc-release/AzurePlayer.exe --project "D:/Games/Managed Controller/project.azureproject"
```

两个示例使用公开资产与同一引擎 API。
玩法类型归属于 `managed/SampleScripts/`。
新增玩法通过内容类型和项目装配表达。

## 编写和重载脚本

类型继承 `ScriptBehaviour` 并实现所需回调。
`Self` 提供生成的 `AzureObject` API。
组件名称和属性访问遵守运行时元数据。
回调定义见[脚本服务契约](../runtime/script-services.md)。

```csharp
using Azure.Engine;
using System.Text.Json.Nodes;
namespace Example.Content;

public sealed class Marker : ScriptBehaviour {
    public override void Update(double delta) {
        Self.Set("azure.transform", "translation", new JsonArray(4, 5, 6));
    }
}
```

CoreCLR 支持替换用户程序集并验证候选。
成功重载创建新的脚本实例状态。
初始化失败时，有效脚本继续执行。
用户程序集依赖范围为单个程序集和引擎 API。

NativeAOT 类型写入 `managed/SampleScripts/types.json`。
构建工具按该清单生成直接类型注册。
重建后将模块复制到项目并启动新进程。
运行中的模块能力由 `capabilities()` 描述。

## 构建与移动项目

先创建启用托管模块的 Release 安装树。
发布工具复制项目挂载目录中的模块与清单。
包同时包含所需运行依赖及许可文本。

```powershell
cmake --install build/ninja-msvc-release --config Release --prefix "D:/AzureEngine"
python tools/build_game.py --project "D:/Games/Managed Annotation/project.azureproject" --install "D:/AzureEngine" --output "D:/Games/Managed Annotation Package"
python tools/build_game.py --verify "D:/Games/Managed Annotation Package"
```

移动完整目录后，通过 `start-game.cmd` 运行。
CoreCLR 使用目标机器上安装的 .NET 运行库。
NativeAOT 包的托管逻辑包含在原生模块中。
SHA-256 清单校验模块、项目和依赖文件。
