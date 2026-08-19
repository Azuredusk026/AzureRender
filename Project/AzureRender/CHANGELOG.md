# Changelog

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
