# AzureRender

Portfolio-first Vulkan renderer for a stylized character and industrial
science-fiction showcase scene.

M1/CQ-2 Toon Ramp/Shadow v1 is complete. A renderer-owned, versioned 10-row
Ramp Atlas now gives Skin/Face soft ramps and Hair/Fabric/Metal/Eye stepped
ramps. Direct diffuse, ambient, shadow visibility, AO, material shadow tint and
style-mask routing have independent QA views. CQ-3 through CQ-6 and the M2 Hero
quality gate are complete. AR-1 through AR-3.5 Editor Preview v1 are complete;
AR-3.6 is the next implementation task.

AR-0 now provides a versioned `RenderSettings` boundary shared by CLI input,
runtime controls, frame uniforms and capture manifests. Face SDF assets use an
explicit glTF material contract rather than inferred texture semantics. Audit an
asset before CQ-3 authoring with:

```bash
python3 tools/audit_face_sdf_compatibility.py path/to/character.glb
```

The staged renderer/editor architecture is documented in
`docs/RENDERER_MODULARIZATION_PLAN_CN.md`. The fixed near-term execution queue
is in [`docs/ACTIVE_DEVELOPMENT_PLAN_CN.md`](docs/ACTIVE_DEVELOPMENT_PLAN_CN.md);
that document is the only source for the next task order.

The current `S1-S36.2` baseline creates a resizable Windows GLFW surface, selects a
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
Azure Gallery, Endfield Industrial, and Neutral Material Check. The presets
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
S26 adds the first animation runtime. It parses glTF animation
samplers/channels, evaluates translation and scale with STEP/linear interpolation,
evaluates rotation with normalized quaternion slerp, loops the timeline, rebuilds
the node hierarchy, and uploads a new joint palette for the current in-flight
frame. A reproducible four-second procedural idle validates the complete path
without changing the original Unreal asset.
S27 turns that runtime into a presentation workflow. Animation selection and
timeline diagnostics use IME-safe numeric controls, while a dedicated portfolio
orbit preset combines the Endfield Industrial presentation, a tighter full-body
camera, restarted animation, internal outlines, and a slow 39-second turntable.
S28 adds deterministic offline capture. A fixed simulation step drives animation
and camera motion independently of wall-clock/render speed, while Vulkan copies
each completed swapchain image to a numbered lossless PNG. Capture manifests
record the asset, GPU, resolution, frame rate, frame count, duration, animation,
and presentation mode. The accompanying FFmpeg script creates a high-quality
BT.709 H.264 MP4 without overwriting existing output.
S29 adds Vulkan timestamp queries around the Shadow, Main Scene, and Internal
Outline stages. Per-frame query pools avoid cross-frame synchronization hazards,
and an optional JSON report records average per-pass GPU time plus total
average/minimum/maximum time without including CPU work, presentation, or PNG
encoding.
S30 adds reproducible technical-breakdown views. The final post-process can
replace Beauty output with world normals, the isolated depth/normal internal
outline response, or the light-space Shadow Map. Command-line switches expose
the same views and conventional/stylized comparison to deterministic capture,
and capture manifests record the selected diagnostic and style state.
S31 adds an optional renderer-native HUD using `stb_easy_font` geometry and
per-frame host-visible vertex buffers. It reports the GPU, framebuffer,
diagnostic/style state, animation timeline, and live Shadow/Main/Outline/Total
GPU averages without adding a font texture or descriptor set. The HUD is off by
default, and HUD-off capture remains pixel-identical to the S30 Beauty baseline.
S32 adds a frame-addressed technical sequence for deterministic video. It splits
the requested capture into five equal chapters—Beauty, World Normal, Internal
Outline, Shadow Map, and Beauty with HUD—while animation and portfolio orbit
continue across the cuts. The manifest records exact chapter frame ranges.
S33 adds renderer-native centered chapter titles, subtitles, and deterministic
fade-through-dark transitions to that sequence. The final chapter delays the
live GPU HUD until its fade-in completes. These overlays are exclusive to
`--technical-sequence`; normal Beauty capture remains pixel-identical to S30.
S34 packages the verified outputs for portfolio delivery. A reproducible
PowerShell tool generates a 1080p cover, five-view technical contact sheet, and
machine-readable manifest containing paths, sizes, SHA-256 hashes, chapter
metadata, and the scoped 1080p Release GPU timing summary without duplicating
the large source videos.
S35 begins the logic-preserving AzureRender identity migration. The CMake
project, shader target, executable, compile definitions, Vulkan application
metadata, window/HUD branding, and former `VulkanApp` type now use AzureRender
names. Legacy glTF `afterglow*` extras, capture format v1, animation names, and
historical media filenames remain readable and unchanged. The renamed Release
executable reproduces the S30 Beauty baseline pixel-for-pixel.
S35.2 starts the implementation split without changing class ownership. Device,
swapchain, buffer/image, shader-module, and debug support methods now live in
`AzureRenderSupport.cpp`; capture manifests, screenshots, and GPU timing output
live in `AzureRenderCapture.cpp`. `AzureRenderApp` remains the only owning class,
and the split build remains pixel-identical to the S30 Beauty baseline.
S35.3 moves draw submission, per-frame UBO/HUD updates, and command-buffer
recording into `AzureRenderFrame.cpp`. Shared matrix operations and `vkCheck`
now live as inline-only helpers in `AzureRenderInternal.hpp`, removing duplicate
implementations without adding a subsystem or changing resource ownership. Both
build configurations, the public Validation run, the five-chapter technical
probe, and the S30 pixel hash remain unchanged.
S35.4 isolates main/post-process render-pass creation, all graphics-pipeline
creation, and main/post-process framebuffer creation in
`AzureRenderPipeline.cpp`. Pipeline handles, creation order, shader inputs, and
destruction remain owned by `AzureRenderApp`; the new translation unit is an
implementation boundary only. Debug/Release, public Validation, technical
sequence, and the S30 Beauty hash all pass after the split.
S35.5 completes the resource-creation split. Descriptor layouts, pools, and
sets now live in `AzureRenderDescriptors.cpp`; image views, depth/normal/shadow
resources, textures, and vertex/index/uniform/joint/HUD buffers live in
`AzureRenderResources.cpp`. Descriptor bindings, image layouts, allocation
counts, upload order, and cleanup ownership remain unchanged. The main
implementation file is now approximately 1,054 lines, and all four regression
gates continue to pass with an identical Beauty hash.
S35.6 removes the last active dependency on the placeholder project-directory
name. All seven Unreal Python tools now resolve the root from their own script
path, with `AZURERENDER_PROJECT_ROOT` as an explicit override. A clean Debug and
Release build from a temporary differently named root both succeeded; public
Validation passed there, and its private-asset Beauty capture remained
pixel-identical to the established baseline. Historical documentation keeps old
names where they describe completed migrations.
S35.7 completes the identity migration by moving the project root from
`Project/MyVulkanApp` to `Project/AzureRender`. Old CMake caches were retired;
fresh preset-based Debug and Release builds both point at the new source root.
Public Validation, private Beauty, and the five-chapter technical probe pass in
the final location, with the Beauty image still matching the S30 SHA-256
baseline exactly.
S36.1 freezes the final LDR baseline and defines the HDR color-pipeline upgrade.
The selected GPU is now probed for RGBA16F sampled/color-attachment/blend
support without changing the active render path. The implementation design uses
a linear `VK_FORMAT_R16G16B16A16_SFLOAT` Scene Color, 0 EV exposure, and a
deterministic ACES-fitted final composite into the existing SRGB swapchain.
Detailed lifecycle, descriptor, diagnostic-view, and acceptance rules are in
`docs/HDR_TONEMAPPING_DESIGN_CN.md`.
S36.2 activates that design. The main pass now renders into one per-swapchain
RGBA16F Scene Color image, while the final opaque fullscreen compositor samples
Scene Color at binding 3, combines internal outlines in linear HDR, applies a
fixed 0 EV Narkowicz ACES-fitted curve, and writes display-linear color to the
SRGB swapchain. Diagnostic views bypass exposure and tone mapping; HUD, chapter
titles, and fades remain post-tone-map overlays. The new 1080p private-asset
Beauty baseline is `captures/s36_hdr_beauty_v1/frame_000000.png` with SHA-256
`5E8BF8B507FE07F385EAADF563DF40CD3C23FA6A2433156DEFD1BFD6AB829357`.

## Requirements

- Windows 10/11
- CMake 3.20+
- C++17 compiler
- Ninja or Visual Studio generator
- Vulkan SDK with `glslc`
- GLFW available through vcpkg
- tinygltf and stb available through vcpkg
- Dear ImGui with GLFW/Vulkan backends and Docking enabled
- FFmpeg with `libx264` (optional, only for MP4 encoding)

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
.\build\ninja-debug\AzureRender.exe
```

Automated validation smoke run:

```powershell
.\build\ninja-debug\AzureRender.exe --smoke-frames 300
```

Load a specific exported glTF/GLB without changing source code:

```powershell
.\build\ninja-debug\AzureRender.exe `
  --asset ".\assets_private\laevat_static\laevat_static_material.glb"
```

Run the S26 animated private test asset:

```powershell
.\build\ninja-debug\AzureRender.exe `
  --asset ".\assets_private\laevat_skinned\laevat_idle_material.glb"
```

`--asset` and `--smoke-frames` can be used together in either order.

## CQ-0 deterministic character QA

CQ-0 completed on 2026-08-02. The long-term M1 art-quality work uses this fixed
command-line QA harness for every subsequent shader node. It
provides five camera presets, four lighting environments, effect-specific
enabled/disabled/isolation states, resumable batches, compiled-shader hashes,
and reproducible manifest metadata.

See [`docs/CHARACTER_QA_HARNESS_CN.md`](docs/CHARACTER_QA_HARNESS_CN.md) for
the complete matrix and run `tools/run_character_qa.ps1` to generate baseline,
isolation, A/B, or full QA evidence.

## Scene and editor preview

Create a versioned scene document from an asset:

```bash
./build/linux-release/AzureRender \\
  --asset ./assets_public/test_model.gltf \\
  --create-scene ./captures/test.azscene
```

Open the document in the renderer-native editor preview:

```bash
./build/linux-release/AzureRender --editor ./captures/test.azscene
```

The preview uses Dear ImGui Docking with an offscreen Vulkan Viewport, scene
outliner, inspector, asset browser and console panel. The Viewport is resized
independently from the swapchain and supports right-drag orbit, middle-drag
pan, and wheel zoom. `Tab` selects nodes, `[`/`]` changes outline strength,
`-`/`=` changes exposure, and closing the window saves the scene.

CQ-1 Material Classes/Data v1, CQ-2 Toon Ramp/Shadow v1, CQ-3 Face SDF/Overlay
v1, CQ-4 Hair KK v1, CQ-5 Rim/Specular/Emissive/Bloom v1, and the CQ-6
Outline/Final Grade implementation are complete. M2 is complete and the next
fixed task is AR-3.6 Viewport resource isolation. CQ-0 evidence is
kept under `captures/cq0_laevat_baseline_v2`, `cq0_laevat_isolation_v2`,
`cq0_laevat_ab_v1`, and `cq0_review_v1`. These private-asset captures are not
part of a public source package.

Material Class/Data v1 is documented in
[`docs/MATERIAL_SYSTEM_V1_CN.md`](docs/MATERIAL_SYSTEM_V1_CN.md). Use
`--qa-isolation material-id` to inspect the per-primitive class assignment.
CQ-2 adds `style-mask`, `ambient`, `direct-diffuse`, and `shadow-tint`
isolation views. Ramp source data lives in
`assets_public/toon_ramp_profiles.json`; regenerate the sampled atlas with
`python tools/build_toon_ramp_atlas.py`.

## Deterministic portfolio capture

Generate a four-second, 1080p60 lossless frame sequence using the S27 portfolio
camera:

```powershell
.\build\ninja-release\AzureRender.exe `
  --asset ".\assets_private\laevat_skinned\laevat_idle_material.glb" `
  --portfolio `
  --width 1920 `
  --height 1080 `
  --capture-dir ".\captures\portfolio_1080p60" `
  --capture-frames 240 `
  --capture-fps 60
```

The capture directory must be new or empty. Existing output is never deleted or
overwritten. The directory receives `frame_000000.png` through the requested
last frame plus `capture_manifest.json`.

Encode the sequence when FFmpeg is on `PATH`:

```powershell
powershell.exe -NoProfile -ExecutionPolicy Bypass `
  -File ".\tools\encode_capture.ps1" `
  -CaptureDirectory ".\captures\portfolio_1080p60" `
  -OutputPath ".\captures\AzureRender_Portfolio_1080p60.mp4"
```

`-FfmpegExecutable <path>` can select a portable FFmpeg executable. The encoder
uses H.264 High Profile through `libx264`, CRF 15, the slow preset, YUV420p,
BT.709 metadata, and Fast Start. It refuses to replace an existing MP4.

Capture CLI:

- `--width` / `--height`: requested framebuffer size, from 64×64 to 7680×4320
- `--capture-fps`: fixed simulation and output rate, from 1 to 240
- `--capture-frames`: exact number of successful frames to write
- `--capture-dir`: new or empty sequence directory
- `--portfolio`: enter the deterministic S27 presentation before frame zero

The first captured frame represents simulation time zero. Rendering speed,
VSync, or PNG compression time therefore cannot change animation/camera motion.

For the 20-second portfolio variant, use the same command with:

```powershell
  --capture-dir ".\captures\portfolio_1080p60_20s" `
  --capture-frames 1200 `
  --capture-fps 60
```

At 60 fps this produces exactly 20 seconds. With the current `0.16 rad/s`
portfolio orbit it covers approximately 183 degrees and five complete loops of
the four-second procedural idle.

## GPU timing

Collect a reproducible 1080p Release timing window:

```powershell
.\build\ninja-release\AzureRender.exe `
  --asset ".\assets_private\laevat_skinned\laevat_idle_material.glb" `
  --portfolio `
  --width 1920 `
  --height 1080 `
  --gpu-timing `
  --gpu-timing-output ".\captures\gpu_timing_1080p.json" `
  --smoke-frames 600
```

`--gpu-timing` prints the Shadow, Main Scene, and Internal Outline averages and
the total average/minimum/maximum GPU duration. `--gpu-timing-output` implies
timing and writes the same measurements to JSON. The output path must not
already exist. These timestamps cover only the three GPU rendering stages; they
do not measure presentation, CPU frame preparation, capture readback, or PNG
encoding, so the total must not be presented as end-to-end frame time.

## Technical breakdown capture

Select a diagnostic output with:

```powershell
.\build\ninja-release\AzureRender.exe `
  --asset ".\assets_private\laevat_skinned\laevat_idle_material.glb" `
  --portfolio --width 1920 --height 1080 `
  --diagnostic-view normal `
  --capture-dir ".\captures\world_normal" `
  --capture-frames 1 --capture-fps 60
```

Accepted views:

- `beauty`: normal final composition
- `normal`: encoded world-space geometric normals
- `outline`: isolated depth/normal internal-outline response
- `shadow`: the 2048×2048 light-space depth map with a display-only contrast curve

`--no-stylized` reproduces the F9 conventional-lighting comparison, while
`--no-inner-outline` reproduces the F10 state. The PNG manifest records
`diagnosticView`, `stylizedLighting`, `internalOutline`, and `hudEnabled`.
Existing capture directories remain protected from overwrite.

## Runtime HUD

Start with the technical HUD and GPU timing enabled:

```powershell
.\build\ninja-release\AzureRender.exe `
  --asset ".\assets_private\laevat_skinned\laevat_idle_material.glb" `
  --portfolio --hud
```

`--hud` implies `--gpu-timing`. Press `H` to show or hide the panel. The HUD
contains:

- selected Vulkan GPU
- framebuffer resolution
- diagnostic, stylized-lighting, and internal-outline state
- animation name, playhead, duration, and playback state
- running Shadow, Main Scene, Internal Outline, and total GPU averages

The HUD is disabled unless explicitly requested, so existing portfolio captures
stay clean. Enabling it for deterministic capture records `hudEnabled: true` in
the manifest and includes the panel in the PNG sequence. If the HUD is enabled
interactively without GPU timing, the panel clearly reports that timing is
disabled.

## Deterministic technical breakdown video

Generate the 20-second, five-chapter 1080p60 source sequence:

```powershell
.\build\ninja-release\AzureRender.exe `
  --asset ".\assets_private\laevat_skinned\laevat_idle_material.glb" `
  --width 1920 --height 1080 `
  --technical-sequence `
  --capture-dir ".\captures\technical_1080p60_20s" `
  --capture-frames 1200 --capture-fps 60
```

`--technical-sequence` requires deterministic capture, makes the frame count
divisible by five, and implies Portfolio Orbit plus GPU Timing. For 1200 frames,
the chapter ranges are:

- frames 0–239 / 0–4 seconds: Beauty
- frames 240–479 / 4–8 seconds: World Normal
- frames 480–719 / 8–12 seconds: Internal Outline
- frames 720–959 / 12–16 seconds: Shadow Map
- frames 960–1199 / 16–20 seconds: Beauty with live HUD

Animation and camera time remain continuous through every transition. Each
chapter receives a centered renderer-native title and subtitle. At 60 fps the
formal sequence uses a 21-frame fade at both ends of each chapter and a
120-frame title window; the final live HUD appears only after the opening fade.
The manifest sets `technicalSequence: true`, stores the exact
`technicalChapters` array, and records `technicalFadeFrames` and
`technicalTitleFrames`.
Encode with the same `tools/encode_capture.ps1` workflow used by normal
deterministic capture. This produces a separate technical video and never
replaces the clean Beauty version.

The executable expects the compiled shaders in the build directory. CMake builds
them automatically and copies the GLFW runtime beside the executable.

## Portfolio delivery package

Open `portfolio/README_CN.md` for the recommended interview presentation order,
final-media index, concise project description, performance caveat, and asset
distribution notice.

Regenerate the cover, technical contact sheet, and artifact manifest with:

```powershell
powershell.exe -NoProfile -ExecutionPolicy Bypass `
  -File .\tools\build_portfolio_package.ps1 `
  -FfmpegExecutable `
  "C:\tmp\afterglow_ffmpeg_812\ffmpeg-8.1.2-essentials_build\bin\ffmpeg.exe"
```

The tool validates every input before writing, references the existing Beauty
and Technical MP4 files instead of copying them, and refreshes SHA-256 hashes in
`portfolio/portfolio_manifest.json`.

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
| `6` | Start the portfolio slow-orbit presentation |
| `7` / `8` | Select the previous/next animation and restart it |
| `9` | Print the current animation, playhead, duration, and state |
| `0` | Cycle Beauty, World Normal, Internal Outline, and Shadow Map views |
| `H` | Show or hide the technical HUD |
| `Left` / `Right` | Rotate by 5 degrees and pause |
| `F1` | Azure Gallery showcase preset |
| `F2` | Endfield Industrial showcase preset |
| `F3` | Neutral Material Check preset |
| `F4` | Pause or resume the loaded animation |
| `F10` | Toggle screen-space internal outlines |
| `F11` | Restart the loaded animation |
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

Unreal export/inspection tools resolve the project root from their own `tools/`
location. Set `AZURERENDER_PROJECT_ROOT` only when Unreal executes a script
without `__file__` or when output should target another project checkout.

`assets_public/test_model.gltf` is a self-contained project-owned smoke-test
asset. It exercises POSITION, NORMAL, TEXCOORD_0, indexed geometry, an embedded
PNG, a sampler, and a base-color material without depending on private files.

## Current glTF scope

- glTF 2.0 `.gltf` and `.glb` input
- scene/node hierarchy with Matrix or TRS transforms
- one glTF skin with joint hierarchy and inverse bind matrices
- `JOINTS_0` and normalized `WEIGHTS_0` four-weight GPU skinning
- per-frame joint-palette storage buffer with static-asset identity fallback
- glTF translation, rotation, and scale animation channels
- STEP and linear vector interpolation plus normalized quaternion slerp
- looped first-animation playback with pause/resume and restart controls
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
- Beauty/world-normal/internal-outline/shadow-map diagnostic outputs
- optional geometry-based runtime HUD with live per-pass GPU averages
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

Multiple simultaneously blended animations, CUBICSPLINE interpolation, morph
targets, per-triangle transparency sorting/OIT, prefiltered HDR environment maps,
bloom, and mipmap generation are intentionally deferred to later milestones.

The Laevat import result and reproducible export pipeline are documented in
`docs/LAEVAT_ASSET_EXPORT_CN.md`. Milestone decisions and verification results
are recorded in `docs/DEVELOPMENT_LOG_CN.md`.
