# Vulkan Stylized Character Renderer

Portfolio-first Vulkan renderer for a stylized character and industrial
science-fiction showcase scene.

The current `S1-S25` baseline creates a resizable Windows GLFW surface, selects a
Vulkan GPU, enables the Khronos validation layer in Debug builds, manages a
two-frame swapchain loop, uploads device-local vertex/index buffers through
staging buffers, updates a per-frame camera uniform, and loads a textured glTF
2.0 triangle mesh from disk. It decodes the base-color PNG, uploads it to an
sRGB Vulkan image, traverses glTF scenes and node transforms, and renders
multiple primitives with independent materials, depth testing, and simple
directional lighting. Character-oriented material support now includes generated
or imported tangents, linear normal maps, alpha mask/blend modes, double-sided
materials, and automatic bounds-based centering and framing. The first private
Laevat static-character import is now working with 13 mesh primitives,
per-material base-color textures, and converted Unreal BC5 normal maps.
Five cloth/weapon materials also carry converted metallic-roughness maps. The
fragment path now combines normal-mapped direct lighting with roughness-driven
Blinn-Phong highlights and metallic F0 tinting. S9 adds a renderer-owned linear
equirectangular environment texture, normal-directed ambient illumination,
view-dependent environment reflections, Fresnel response, and a roughness-aware
reflection blur approximation.
S10 restores the Unreal packed material's Specular channel and masked Emissive
color through a combined sRGB texture, with per-material emissive intensity
passed through material push constants.
S11 stores a bounds center for every mesh primitive and sorts BLEND primitives
back-to-front by current view-space depth while keeping depth writes disabled.
S12 adds deterministic presentation controls and native Vulkan screenshot
readback from the swapchain to PNG.
S13 adds a proportional inverted-hull outline pass with front-face culling and
depth-test-only composition before the main material passes.
S14 bakes Laevat's per-material metallic strength and signed roughness
adjustment into the glTF material textures, and rebalances the provisional LDR
environment reflection to reduce the washed-out, glass-like appearance.
S14.1 also repairs asset-specific occlusion holes by preserving both sides of
solid Laevat surfaces whose Unreal export contains mixed triangle winding.
S15 carries Unreal's sparse `_M` style masks through the asset, loader, and
descriptor paths, then combines them with a smooth banded diffuse response and
conservative warm edge accents.
S16 exposes the style toggle, mask strength, and diffuse-band threshold through
runtime UBO parameters and IME-safe function-key controls for deterministic
portfolio before/after captures.
S17 restores Unreal material-instance `Lam_Shadow_Color` / `Lam_ShadowColor`
and `AO_Color` as per-material glTF extras and push constants. They tint only
the stylized shadow band, so F9 still provides a clean conventional/stylized
comparison without changing alpha blending or depth behavior.
S18 audits the Laevat face path, rejects a Face SDF implementation because the
asset binds no SDF texture, and restores the face material's authored
`Matcap01` highlight instead. The Matcap uses view-space normals, an independent
material texture binding, a black fallback, and the existing F9 style switch.
S19 adds a deterministic face close-up preset on key 5, with an independent
camera target and distance while preserving the existing full-body views. The
close-up QA also replaces the hard face Matcap edge with a nine-tap softened,
skin-tinted highlight suitable for portfolio captures.
S20 restores the hair material's packed `_HN` data as separate base/highlight
normals and uses it for a restrained Kajiya-Kay-style highlight.
S21 adds a renderer-generated circular showcase platform, fullscreen procedural
background, and key/fill/rim presentation lighting.
S22 turns the showcase into three runtime-selectable presentation presets:
Afterglow Gallery, Endfield Industrial, and Neutral Material Check. The presets
change the background, platform tint, and key/fill/rim response without modifying
the imported character asset.
S23 adds a 2048×2048 directional Shadow Map, alpha-aware shadow casters,
raster depth bias, and manual 3×3 PCF filtering. Character shadows now reach the
runtime showcase platform while direct key-light specular and hair highlights
remain consistent with the same light direction.
S24 adds a sampled per-swapchain normal attachment, reuses the sampled scene
depth attachment, and composites true screen-space internal outlines in a
second render pass. Depth and normal discontinuities now reinforce clothing and
mechanical structure without replacing the existing inverted-hull silhouette.
Hair and face materials carry reduced participation weights to avoid tracing
every authored normal island in close-up views.
S25 adds the first GPU skinning foundation. The loader now accepts glTF
`JOINTS_0`, `WEIGHTS_0`, one skin, and inverse bind matrices, calculates the bind
pose joint palette, and uploads it through a per-frame storage buffer. Material,
inverted-hull outline, and shadow vertex shaders use the same four-weight skin
transform. Assets without a skin continue through a one-joint identity fallback.

## Requirements

- Windows 10/11
- CMake 3.20+
- C++17 compiler
- Ninja or Visual Studio generator
- Vulkan SDK with `glslc`
- GLFW available through vcpkg
- tinygltf and stb available through vcpkg

## Configure and build

This workstation currently uses:

- Vulkan SDK: `C:\VulkanSDK\1.4.350.0`
- vcpkg toolchain:
  `C:\Users\23587\.vcpkg-clion\vcpkg\scripts\buildsystems\vcpkg.cmake`
- CLion MinGW and Ninja

Example Debug configuration:

```powershell
$env:PATH = "D:\JetBrains\CLion 2026.1\bin\mingw\bin;$env:PATH"

cmake -S . -B build/ninja-debug -G Ninja `
  -DCMAKE_BUILD_TYPE=Debug `
  -DCMAKE_C_COMPILER="D:/JetBrains/CLion 2026.1/bin/mingw/bin/gcc.exe" `
  -DCMAKE_CXX_COMPILER="D:/JetBrains/CLion 2026.1/bin/mingw/bin/g++.exe" `
  -DCMAKE_MAKE_PROGRAM="D:/JetBrains/CLion 2026.1/bin/ninja/win/x64/ninja.exe" `
  -DCMAKE_TOOLCHAIN_FILE="C:/Users/23587/.vcpkg-clion/vcpkg/scripts/buildsystems/vcpkg.cmake"

cmake --build build/ninja-debug
```

Run:

```powershell
.\build\ninja-debug\MyVulkanApp.exe
```

Automated validation smoke run:

```powershell
.\build\ninja-debug\MyVulkanApp.exe --smoke-frames 300
```

Load a specific exported glTF/GLB without changing source code:

```powershell
.\build\ninja-debug\MyVulkanApp.exe `
  --asset ".\assets_private\laevat_static\laevat_static_material.glb"
```

`--asset` and `--smoke-frames` can be used together in either order.

The executable expects the compiled shaders in the build directory. CMake builds
them automatically and copies the GLFW runtime beside the executable.

## Presentation controls

| Key | Action |
|---|---|
| `Space` | Pause or resume automatic rotation |
| `R` | Resume automatic rotation |
| `1` | Calibrated front view and pause |
| `2` | Right-side view and pause |
| `3` | Back view and pause |
| `4` | Left-side view and pause |
| `5` | Face close-up view and pause |
| `Left` / `Right` | Rotate by 5 degrees and pause |
| `F1` | Afterglow Gallery showcase preset |
| `F2` | Endfield Industrial showcase preset |
| `F3` | Neutral Material Check preset |
| `F10` | Toggle screen-space internal outlines |
| `F9` | Toggle all stylized lighting layers |
| `F7` / `F8` | Decrease/increase style-mask strength |
| `F5` / `F6` | Decrease/increase diffuse-band threshold |
| `F12` | Save the current swapchain image as PNG |

Screenshots are written to `captures/capture_<timestamp>.png`. The directory is
ignored because local captures may contain private character assets.

## Baseline acceptance checks

- Debug and Release configurations build.
- Debug uses `VK_LAYER_KHRONOS_validation`.
- The window displays a rotating cyan-blue-purple 3D mesh on a dark blue background.
- Startup reports the loaded glTF vertex, index, primitive, and material counts.
- Resizing and minimizing/restoring the window do not crash.
- Closing and relaunching the application is stable.
- RenderDoc can attach or capture through its installed Vulkan layer.

## Asset boundary

`assets_private/` is ignored by version control. The original Unreal Engine
character assets remain outside this repository. Public builds use
`assets_placeholder/` until redistribution permission is established.

`assets_public/test_model.gltf` is a self-contained project-owned smoke-test
asset. It exercises POSITION, NORMAL, TEXCOORD_0, indexed geometry, an embedded
PNG, a sampler, and a base-color material without depending on private files.

## Current glTF scope

- glTF 2.0 `.gltf` and `.glb` input
- scene/node hierarchy with Matrix or TRS transforms
- one glTF skin with joint hierarchy and inverse bind matrices
- `JOINTS_0` and normalized `WEIGHTS_0` four-weight GPU skinning
- per-frame joint-palette storage buffer with static-asset identity fallback
- multiple triangle-list meshes and primitives
- multiple base-color materials
- TANGENT input or automatic tangent generation
- linear normal textures and tangent-space normal mapping
- glTF metallic-roughness textures (G = roughness, B = metallic)
- per-material offline metallic strength and signed roughness adjustment
- per-fragment view vector and roughness-driven specular lighting
- global equirectangular environment texture in linear color space
- environment diffuse, Fresnel reflection, and roughness-aware reflection
- warm key light, cool fill light, and view-dependent rim lighting
- fullscreen procedural gradient, halo, and vignette background
- bounds-derived procedural showcase platform with contact-darkening
- three runtime showcase presets for gallery, industrial, and neutral inspection
- 2048×2048 directional Shadow Map with alpha cutout and 3×3 PCF
- synchronized character self-shadow and showcase-platform cast shadow
- sampled scene depth and R8G8B8A8 screen-space normal attachment
- depth/normal internal-outline post-process with per-material participation
- runtime internal-outline toggle for deterministic comparison captures
- packed Specular level and masked Emissive color
- sparse style-mask texture and smooth banded diffuse lighting
- Unreal hair `_HN` data as RG base normal plus BA highlight normal
- material-driven Kajiya-Kay-style tangent highlight for hair
- opaque-first rendering and per-frame back-to-front BLEND primitive sorting
- pause/resume, four calibrated view presets, and 5-degree angle adjustment
- runtime style toggle, mask-strength adjustment, and diffuse-band threshold
- native BGRA/RGBA swapchain readback and PNG screenshot output
- proportional geometry outline based on the imported asset extent
- OPAQUE, MASK, and BLEND alpha modes
- double-sided materials
- bounds-based automatic model fitting
- float POSITION, NORMAL, and TEXCOORD_0
- unsigned 8/16/32-bit indices
- base-color texture with an RGBA fallback texture

Animation playback, morph targets, per-triangle transparency sorting/OIT,
prefiltered HDR environment maps, bloom, and mipmap generation are intentionally
deferred to later milestones.

The Laevat import result and reproducible export pipeline are documented in
`docs/LAEVAT_ASSET_EXPORT_CN.md`. Milestone decisions and verification results
are recorded in `docs/DEVELOPMENT_LOG_CN.md`.
