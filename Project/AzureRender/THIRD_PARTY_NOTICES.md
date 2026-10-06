# 第三方许可说明

AzureRender 使用以下第三方依赖。发布目录保留包管理器或上游分发提供的许可证文件。本文说明第三方依赖的版权与分发许可。

离线动画采样工具使用 [ufbx](https://github.com/ufbx/ufbx/tree/v0.17.1) 0.17.1，许可证为 MIT。`prepare_fbx_tool.py` 校验源码 SHA-256，下载许可证并编译宿主工具。源码与许可证位于本机 `build/f1/ufbx/`。该工具用于素材准入与离线重定向，运行时读取 glTF 2.0。

| 依赖 | 许可证 |
| --- | --- |
| Vulkan Headers 与 Loader | Apache License 2.0 |
| GLFW | zlib/libpng |
| Dear ImGui | MIT |
| tinygltf | MIT |
| stb | MIT 或公有领域双许可 |
| OpenEXR 与 Imath | BSD-3-Clause |
| libdeflate | MIT |
| OpenJPH | BSD-2-Clause |
| nlohmann/json | MIT |
| Jolt Physics | MIT |
| Lua 与 sol2 | MIT |
| RmlUi | MIT |
| miniaudio | MIT-0 |
| FreeType | FreeType License |
| libpng、zlib、bzip2、Brotli | 对应上游许可随安装树分发 |
| LatoLatin | SIL Open Font License 1.1 |

Dear ImGui 使用 docking 分支 1.92.8，源码位于 `third_party/imgui`。它由项目工具链编译，保证库与项目的 ABI 一致。许可证保存在 `third_party/imgui/LICENSE.txt`。

发布包使用 `assets_public/` 中的项目公开资源。私有角色资产与派生捕获由本机授权范围管理。

## Jolt Physics

[Jolt Physics 5.6.0](https://github.com/jrouwe/JoltPhysics/tree/v5.6.0) 以静态库链接。发布许可证路径为 `licenses/joltphysics-LICENSE.txt`。本地封装使用 PhysicsSystem、CharacterVirtual、碰撞过滤和查询的公开 API。

## Lua 与 sol2

[Lua 5.4.8](https://www.lua.org/versions.html) 与 [sol2 3.5.0](https://github.com/ThePhD/sol2/tree/v3.5.0) 通过 vcpkg 安装。sol2 端口修订号为 1，Lua 使用 manifest 版本覆盖固定为 5.4.8。

许可证安装为 `licenses/lua-LICENSE.txt` 和 `licenses/sol2-LICENSE.txt`。本地绑定使用状态、环境、保护函数和指令钩子的公开 API。

MSVC C5321 在包含 sol2 头时局部关闭，适用对象为 UTF-8 表情符号字面量。项目代码继续使用严格警告。上游版本与本地适配由 Debug、Release 和 Player 验证。

## 游戏界面与音频

[RmlUi 6.3](https://github.com/mikke89/RmlUi/tree/6.3) 提供文档布局与字体界面。本地 Vulkan 适配使用公开 RenderInterface，资源由项目 RHI 管理。

[miniaudio 0.11.25](https://github.com/mackron/miniaudio/tree/0.11.25) 提供音频引擎和解码。本项目选择 MIT-0 许可。实现头在独立编译单元内处理上游警告。

FreeType 与其压缩依赖经 vcpkg 安装。安装树保留 RmlUi、miniaudio、FreeType、libpng、zlib、bzip2 和 Brotli 的许可证。

`assets_public/fonts/LatoLatin-Regular.ttf` 来自 RmlUi 6.3 示例。原始许可保存在同目录，并安装为 `licenses/LatoLatin-LICENSE.txt`。公开 WAV 为项目生成的衰减正弦音效。

Windows 游戏包随 Player 分发运行库、着色器、字体与第三方许可。`game_manifest.json` 为每份许可记录文件大小与 SHA-256。模板使用项目公开模型、脚本和音效，Python 发布工具由项目自行实现。

## Windows 编译器运行库

`assets_public/third_person/material_fixture.gltf`、`fixture_hair_data.png` 和生成工具由本项目自行制作，采用项目 MIT 许可。Face SDF 使用项目已有公共纹理。私有主角与用户提供动作保存在本机输入范围。

MSVC 构建随安装树分发 Visual C++ 可再分发运行库。CMake 从当前工具链确定 DLL，VS2022 使用 VC143 目录。使用范围遵循 [Visual Studio 可分发代码条款](https://learn.microsoft.com/visualstudio/releases/2022/redistribution)，本机验收工具链为 MSVC 14.44。

## 编辑器字体

`assets_public/fonts/NotoSansCJKsc-Regular.otf` 使用 Noto Sans CJK SC Regular。来源为 [Noto CJK 字体仓库](https://github.com/notofonts/noto-cjk)。该字体覆盖中文项目名、对象名与资源名。

字体采用 SIL Open Font License 1.1。完整许可位于同目录的 `NotoSansCJK-LICENSE.txt`。安装树保留该字体与许可，并记录文件哈希。

## gkNextEngine

UI 作用域参考 gkNextEngine 的 `UiScopes.hpp`。
来源提交为 `4ba5b7cd106c282e7ed166ff853aeea87b392680`。
版权所有为 2024 至 2026 年 gameknife。
适配模式使用 MIT 许可。

源码路径为 `src/Modules/NextUI/UI/UiScopes.hpp`。
本地适配位于 `src/editor/ui/UiScopes.hpp`。
许可保存于 `third_party/gknextengine/LICENSE.txt`。
发布安装保留 `licenses/gkNextEngine-LICENSE.txt`。

控件、面板与设置采用本项目的公共契约。
参考设计包括类型、来源与帧边界应用。
验收依据见[U4 实施步骤](docs/plans/2026-10-06-ai-native-editor-steps.md#u4-ui-基础与设置体系)。

开发期资源服务参考相同提交的 B6 与 B7。
参考路径为 `src/Modules/LiveCoding/ShaderHotReloader.cpp`。
视图参考路径为 `src/Engine/Rendering/Preview/RenderViewServices.hpp`。
缩略图参考位于 `src/Application/Editor/Common/Preview/AssetThumbnailRenderer.hpp`。

本项目按 RHI、帧图与生产操作契约自行适配。
着色编译器由当前 Vulkan SDK 提供。
完整采用范围见[开发期资源服务](docs/runtime/development-previews.md)。
