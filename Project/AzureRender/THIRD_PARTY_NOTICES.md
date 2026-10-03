# 第三方许可说明

AzureRender 使用以下第三方依赖。发布目录保留包管理器或上游分发提供的许可证文件。本文说明第三方依赖的版权与分发许可。

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

Dear ImGui 使用 docking 分支 1.92.8，源码位于 `third_party/imgui`。它由项目工具链编译，保证库与项目的 ABI 一致。许可证保存在 `third_party/imgui/LICENSE.txt`。

发布包使用 `assets_public/` 中的项目公开资源。私有角色资产与派生捕获由本机授权范围管理。

## Jolt Physics

[Jolt Physics 5.6.0](https://github.com/jrouwe/JoltPhysics/tree/v5.6.0) 以静态库链接。发布许可证路径为 `licenses/joltphysics-LICENSE.txt`。本地封装使用 PhysicsSystem、CharacterVirtual、碰撞过滤和查询的公开 API。

## Lua 与 sol2

[Lua 5.4.8](https://www.lua.org/versions.html) 与 [sol2 3.5.0](https://github.com/ThePhD/sol2/tree/v3.5.0) 通过 vcpkg 安装。sol2 端口修订号为 1，Lua 使用 manifest 版本覆盖固定为 5.4.8。

许可证安装为 `licenses/lua-LICENSE.txt` 和 `licenses/sol2-LICENSE.txt`。本地绑定使用状态、环境、保护函数和指令钩子的公开 API。

MSVC C5321 在包含 sol2 头时局部关闭，适用对象为 UTF-8 表情符号字面量。项目代码继续使用严格警告。上游版本与本地适配由 Debug、Release 和 Player 验证。
