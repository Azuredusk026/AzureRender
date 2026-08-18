# Private assets

This directory is excluded from version control except for this notice and its
`.gitignore`. Local files remain available for optional QA but are absent from
fresh clones, source archives, CI, install trees, and release packages.

The first private test character is Laevat (莱万汀), exported from a local
Unreal project. Machine-specific source paths are intentionally undocumented.

Do not commit third-party character meshes, textures, derived exports, or
redistributable archives unless their license has been verified explicitly.

First private character export target:
`assets_private/laevat_static/laevat_static.glb`.

See `docs/ASSET_AND_VISUAL_QA_CN.md` for the current export and validation rules.

CQ-1 Material Class/Data v1 validation uses the generated private asset
`laevat_skinned/laevat_skinned_material_cq1_v2.glb`. It embeds versioned
`extras.azureRenderMaterial` profiles and remains excluded from version
control. Historical asset audits are retained under `docs/archive/`.
