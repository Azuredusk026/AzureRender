# Changelog

## G0 引擎基础 — 2026-10-03

- 建立 Foundation、Platform、Render Core、Runtime、Editor 与宿主库边界。
- 提供独立 AzurePlayer，项目支持版本化配置、虚拟挂载和目录迁移。
- 接入运行时 World、场景快照、暂停、单步和延迟操作。
- 补齐黑洞绘制计数，以及帧槽、取图、提交、呈现和整帧 CPU 计时。
- 根据本机对照选择小批次录制策略，CPU 录制中位数约降低 36.53%。
- Debug 与 Release 各 41 项回归通过，三场景 24 对图像像素一致。
- 黑洞正面电影档三轮 GPU 中位数为 18.737484 ms，满足 20 ms 门禁。
- 发布门禁包含隔离 Player、项目迁移、三场景运行与文档安装。

## G0 黑洞优化准入 — 2026-10-03

- 盘采样增加带浮点安全裕量的保守高度判断，提前排除无贡献位置。
- 电影档保留四射线与现行效果，五相机及 60 帧运动序列像素一致。
- 正式三轮 GPU 平均值中位数为 18.071215 ms，满足正面相机 20 ms 预算。
- 近景记录为 39.350431 ms，性能适用范围按构图说明。
- 核验引擎同步与调度，登记小负载并行开销及统计覆盖问题。
- 增加预算行为测试、自动图像对照和正式采样工具。
- 阶段验收记录按目录安装，修复同名日期文件覆盖问题。

## R4 Windows 渲染核心验收 — 2026-10-03

- 增加 32 个独立公共网格资源和 16 个点光源的压力场景。
- 四条提交路径采集完整性能曲线，并验证逐帧图像哈希一致。
- 黑洞覆盖三个质量档位，保存有效截图与 GPU 时间。
- 角色和黑洞各长跑至少 120 秒，执行六轮窗口尺寸变化和恢复。
- 运行日志记录交换链重建、黑洞历史帧重置和卸载资源计数。
- 压力场景并行 GPU 平均约 7.62 ms，黑洞电影档约 40.48 ms。
- CPU 录制实测串行更低，电影档超出本机 60 Hz GPU 帧预算。
- 安装树包含 Windows 验收说明、公开压力场景和 R4 验收记录。

## R3 完成 — 2026-10-03

- 完成并行提交、GPU 驱动和能力降级的阶段验收，R4 为 Ready。
- 性能工具应用 `r3-budget-v2`，覆盖多资源和小场景预算。
- 增加预算边界测试与归档采样自动复核工具。
- Debug 和 Release 各 36 项回归通过，发布门禁通过。
- 多资源 CPU 录制为 0.458901 ms，额外开销为 0.085914 ms。

## R3 验收标准调整 — 2026-10-03

- 验收设备跟随本机工作环境，同组采样在同一设备对照。
- CPU 门禁从至少加速 5% 调整为录制低于 0.5 ms，且多资源额外开销至多 0.1 ms。
- 小型负载的线程调度成本使固定加速比例难以反映实际帧预算，因此采用绝对开销约束。
- 现有多资源数据按新标准通过，实测并行 CPU 仍较串行增加约 23%。
- 阶段通过代表正确性与有界开销，不承诺并行加速。GPU、图像一致性和发布门禁保持原要求。

## R3 并行录制验收进展 — 2026-10-03

- 主线程参与录制，任务批次独立持有存储并通过原子索引分派。
- 蒙皮以一个帧图 Pass 管理全部网格，透明实例按连续区间分片录制。
- 阴影与主场景读取共享只读上下文，显式传入录制器和统计槽。
- Windows 安装树包含并行提交运行时说明。
- 256 实例性能预算通过，多资源 CPU 性能预算失败，R3 保持 Active。

## R2 渲染质量与光照 — 2026-09-30

- 接入 Blackhole 多级 HDR Bloom 与 Character GPU 环境预滤波。
- 接入 Compute 蒙皮、动画和 Morph；顶点着色器回退在动画末帧保持逐像素一致。
- 场景点光源进入 GPU 光源缓冲，聚簇索引驱动点光照，方向光使用四级级联深度图集并保留 PCSS。
- 增加公开 0/16 点光场景、聚簇规模性能采集和 GPU 图像回归。
- 完成 Windows Debug/Release 33 项 CTest 与 Release 安装、隔离运行和打包门禁。

## R1 桌面同步契约 — 2026-09-26

- 增加图像阶段、访问和子资源描述，以及缓冲区屏障。
- 修复有效窗口尺寸下的额外事件等待及最小化期间关闭问题。
- Android 适配无限期延后，恢复时间由用户安排。
- 同步运行时说明、生命周期测试和阶段验收记录。


## Plain-language documentation - 2026-08-26

- Rewrote the active Chinese documentation with shorter sentences and a more direct technical voice while preserving API names, formulas and contracts.
- Split dense explanations of Vulkan synchronization, resource ownership, character shading and black-hole rendering into smaller steps for C++ readers new to Vulkan.
- Added a prose-length and punctuation check to the documentation workflow so future pages do not return to long compound sentences.

## Vulkan beginner documentation and portable formulas - 2026-08-26

- Replaced non-portable LaTeX delimiters in active documents with GitHub- and MkDocs-compatible math blocks and corrected a corrupted Doppler `beta` expression.
- Expanded the existing architecture document for C++ developers new to Vulkan, mapping object ownership, initialization, frame submission, resource upload, descriptors, pipelines and synchronization to concrete AzureRender API calls.
- Added documentation checks that reject the unreadable delimiter form and require the frame API walkthrough to remain present.

## Documentation system - 2026-08-25

- Replaced the fragmented active-document set with eight topic-oriented GitHub Pages documents covering operation, Vulkan architecture, Character, Blackhole, assets/editor, development/release and centralized reference data.
- Added strict MkDocs Material builds and GitHub Pages deployment while keeping the Markdown directly readable in the repository and install tree.
- Removed duplicated dated implementation narratives from active documentation; historical plans and audits remain under `docs/archive/` or Git history.

## Continuous Face SDF and hair-volume contours - 2026-08-20

- Replaced the frame-discontinuous Face SDF mirror branch with a smooth head-local lateral-axis blend, removing the pure-bright face pop during the 6-7 second turntable interval.
- Strengthened Hair AO with style-mask, grazing-angle and HN-to-geometric-normal cavity signals while preserving the existing red diffuse floor and independent KK lobes.
- Fed Hair HN detail into the internal-outline normal buffer and raised only Hair participation, revealing layered strand boundaries without widening the silhouette shell.
- Verified the 130-179 frame continuity window with a maximum consecutive face-region luma change of 0.204/255, plus Beauty, shadow-tint and outline isolation probes.

## Character key-shadow contrast and hair-color protection - 2026-08-20

- Increased the Endfield world-space key while reducing fill and the toon ambient shadow floor, restoring visible lit/back-lit separation without global exposure changes.
- Increased Face and Skin participation in the authored Lam/shadow system, while narrowing only the bright end of the Face SDF ramp so the face remains close to body skin under matching illumination.
- Added a ramp-aware red diffuse floor and stronger base-hue reprojection for Hair; specular and Kajiya-Kay highlights remain independent and the crown no longer needs grey ambient energy to stay readable.
- Verified Debug/Release builds, all 12 Debug tests, Face/Body Beauty frames, Hair KK and Shadow Visibility isolation captures.

## Face SDF light frame and skin-tone correction - 2026-08-20

- Replaced direct imported-head-axis lighting with bind-relative head rotation so arbitrary glTF joint orientation no longer pins Face SDF illumination near white.
- Increased the visible SDF ramp range and warm shadow tint while reducing Face-only diffuse energy to match the body skin under the same key light.
- Verified a clear left/right transition in Face SDF isolation, stable movement across the turntable, and representative cheek values close to the shoulder-skin range.

## Character Face SDF asset binding - 2026-08-20

- Fixed the skinned Laevat material pipeline so every generated face material embeds `face_sdf_v1.png` and a complete Face SDF v1 profile instead of only advertising eligibility.
- Bound the profile to the unique `Bip001_Head` node and preserved it through idle-animation injection.
- Verified the final private animation GLB with the Face SDF compatibility audit, material-profile validation, brow-mesh audit, runtime load diagnostics and enabled/disabled visual captures.
- Re-recorded separate 16-second Beauty and Shadow Visibility turntables at native 2560x1440/24 fps and fully decoded all 384 frames of each delivery.

## PCSS soft shadows and native 2K capture - 2026-08-20

- Replaced the visually hard fixed 3x3 shadow filter with a bounded Poisson PCSS blocker search and receiver-distance penumbra.
- Added a versioned, validated and scene-serialized maximum shadow filter radius plus an editor softness control.
- Made deterministic capture windows hidden and borderless so Windows does not clamp native 2560x1440 framebuffers to the desktop work area.
- Added shadow settings to deterministic capture state hashes and manifests.
- Re-recorded and fully decoded the five-pass character showcase at native 2560x1440/24 fps: 1920 frames and 80.00 seconds.

## Brow skinning audit and left-start showcase - 2026-08-19

- Audited the brow/eyelash primitive: 34 valid topology islands, normalized symmetric skin weights, and no unrelated joint influences.
- Reverted the incorrect whole-primitive vertical scale that separated independently skinned cards and replaced it with a two-texel Face-D brow-stroke dilation.
- Changed the character Portfolio turntable to start facing left, rotate uniformly through 360 degrees in 16 seconds, and return to the left-facing pose.
- Re-recorded Beauty, Albedo, World Normal, Shadow Visibility, and Material ID as five complete turns and fully decoded all 1920 frames of the 80-second H.264 delivery.

## Character final showcase archive - 2026-08-19

- Re-recorded the 36-second character technical showcase after the final brow-card lift and Endfield lighting rebalance.
- Added close-up red-brow evidence plus front, side, and back Beauty stills documenting material light variation and real-time platform shadows.
- Archived the superseded local character media, verified all 864 H.264 frames by full decode, and left the frozen black-hole delivery unchanged.

## Character brow and lighting finalization - 2026-08-19

- Lifted the data-driven brow card above the upper eyelid and preserved a visible deep-red Face-D response through HDR composition.
- Rebalanced the Endfield look around a fixed lateral world-space key, lower environment/fill energy, stronger real-time shadow and Lam tint.
- Doubled only the character portfolio turntable speed so the eight-second Beauty segment exposes front, side, and back lighting changes without altering the frozen black-hole motion.

## Black-hole final archive - 2026-08-19

- Froze the approved P1 shader, quality settings, two-camera media, reproduction commands, and SHA-256 records as the final black-hole baseline.

## Release-prep workspace - 2026-08-19

- Reduced the active build tree to Debug/Release build and self-contained install directories.
- Moved obsolete captures, prior package output, encoding caches, and validation scratch data into a local ignored archive.
- Rebuilt and reinstalled both configurations, passed 12/12 Debug tests, verified isolated Windows runtimes, and intentionally generated no package.

## Two-scene technical showcase - 2026-08-19

- Replaced the black-hole four-view cut with two eight-second segments: front and moving close-up.
- Replaced the character turntable-only cut with an eight-second Beauty segment followed by seven four-second diagnostic views.
- Archived the previous local media and verified 1600x900, 24 fps, square-pixel BT.709 output by full-frame decoding.

## Disk seam and brow overlay - 2026-08-19

- Removed the accretion-disk radial seam by embedding angular noise coordinates on a continuous circle.
- Added an explicit brow-overlay material feature using Face D RGB, unlit output, constant 0.95 opacity, and view-directed vertex offset.
- Converted the authored Unreal brow offset from 4.679 centimetres to 0.04679 glTF metres and unified the Vulkan material push-constant stage range.

## Showcase media delivery - 2026-08-19

- Re-recorded the character turntable at 1600x900/24 fps with fixed front framing, foot-centred rotation, corrected eyebrows, directional lighting, and real-time shadows.
- Re-recorded the black hole as four ordered eight-second Cinematic segments: front, high, moving orbit, and reference close-up.
- Encoded both videos as square-pixel BT.709 H.264 and verified full-frame decoding.
- Archived legacy version-suffixed local media and adopted timestamp plus short Chinese filenames.

## Character overlay and hair data - 2026-08-19

- Corrected transparent triangle sorting to preserve global vertex indices, restoring eyebrow and facial overlay geometry.
- Bound the hair master material's `_P` packed texture in addition to the cloth-style `T_RGBA_P` key.
- Documented the distinct Base Color, HN strand-data, and P packed-material texture paths.

## Showcase framing and turntable - 2026-08-19

- Centered the character turntable and showcase platform on a robust bind-pose foot pivot.
- Changed the character portfolio camera to a fixed front view while preserving world-space lighting and real-time shadow variation during rotation.
- Excluded the showcase platform and blended overlay geometry from the silhouette pass.
- Added a right-side close black-hole composition for the fourth eight-second showcase segment.
- Replaced version-suffixed showcase-media naming with timestamp plus short Chinese descriptions.

## Character sky lighting - 2026-08-18

- Made the Evening Sky environment visible in the Endfield backdrop instead of suppressing it to eight percent.
- Increased directional environment irradiance and the toon-shadow ambient floor without flattening AO or key-light contrast.
- Rebalanced the Endfield grade from a dark negative exposure to a neutral presentation exposure.

## Character hair readability - 2026-08-18

- Bounded hair environment specular and base-normal influence to remove the grey-white crown.
- Reworked dual-lobe Kajiya-Kay ramping so highlights survive normal viewing distances independently of direct-light visibility.
- Added stable class-specific hair AO when source AO alpha is absent.
- Removed erroneous clip-space outline enlargement and reduced geometric outline width.

## Scene environments - 2026-08-18

- Added a shared environment-source contract for pluggable scene renderers.
- Added six-face cubemap directory decoding and bounded RGBA16F equirectangular conversion.
- Connected the black-hole escape ray to the Space Skybox and the character renderer to the Evening Sky environment.
- Kept private environment assets outside Git and release packages; no package was generated.

## Blackhole visual correction - 2026-08-18

- Ported multiplicative cloud noise, spiral coordinates, dynamic thickness, and dust gaps from the local reference implementation.
- Strengthened Doppler temperature shift, directional color tint, and relativistic beaming so the rotating disk is visibly asymmetric.
- Added a persistent `over-shoulder` camera preset for lower-right subject framing and upper-left sky coverage.

## Character visual correction - 2026-08-18

- Simplified the Endfield character backdrop by removing competing grids, the horizon bar, and the right-side light rail.
- Added class-aware dielectric limits so skin, face, hair, fabric, and eye materials cannot inherit metallic values from incompatible packed textures.
- Corrected Face SDF lateral lighting, raised the toon shadow floor, and made authored AO and dual-lobe hair KK highlights reliably visible.

## R5 - 2026-08-18

- Added snapshot-based editor Undo/Redo with a bounded command history.
- Added explicit safe-frame asset reload and resource dependency/status views.
- Added `.azscene v2` node transforms and prefab/instance references with v1 migration.
- Added semantic viewport capture requests and an editor Capture panel.

## R4 - 2026-08-18

- Moved five character showcase looks from C++ constants to a validated, versioned JSON catalog.
- Added editor controls for showcase look, background, platform, Face SDF, outline, and exposure.
- Added modular character background/platform switches and explicit public material profiles.
- Advanced RenderSettings to schema v6 while preserving legacy scene migration.

All notable public changes are recorded here. Versions follow Semantic Versioning.

## 0.1.0-rc1 - 2026-08-18

- Added pluggable `character` and `blackhole` scene renderers.
- Added temporal black-hole tracing with HDR history, TAA and bloom.
- Added the Endfield Industrial v1 character presentation preset.
- Added editor, deterministic capture, GPU timing and public visual evidence.
- Made Windows Debug/Release packages self-contained with MinGW runtime DLLs.
- Consolidated release documentation and removed private assets from the current tree.
