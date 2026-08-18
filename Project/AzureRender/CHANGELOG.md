# Changelog

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
