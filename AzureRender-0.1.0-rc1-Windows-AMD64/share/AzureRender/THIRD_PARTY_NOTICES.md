# Third-Party Notices

AzureRender links or includes the following dependencies. Release builds must preserve the license files supplied by the package manager or upstream distribution.

- Vulkan Headers and Loader: Apache License 2.0
- GLFW: zlib/libpng license
- Dear ImGui: MIT License
- tinygltf: MIT License
- stb libraries: MIT License or public-domain dual option

Dear ImGui is vendored under `third_party/imgui` (docking branch, version 1.92.8) and
compiled from source with the project toolchain so the library ABI always matches the
project compiler. Its MIT license text is preserved at `third_party/imgui/LICENSE.txt`.
The vcpkg imgui package is not used on Windows/MinGW builds because its MSVC-compiled
static library is not linkable from the MinGW toolchain.

The public demo under `assets_public/` is project-owned. Private character assets and derived captures are intentionally excluded from release packages.

No license grant for the AzureRender project source itself is established by this notice.
