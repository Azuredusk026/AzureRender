# Changelog

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
