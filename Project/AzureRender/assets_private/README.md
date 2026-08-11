# Private assets

This directory is intentionally excluded from version control.

The first private test character is Laevat (莱万汀), exported from the local
Unreal project at `D:\Epic\UE Project\ZMDRender\Content\ZMD\莱万汀`.

Do not commit third-party character meshes, textures, derived exports, or
redistributable archives unless their license has been verified explicitly.

First private character export target:
`assets_private/laevat_static/laevat_static.glb`.

See `docs/LAEVAT_ASSET_EXPORT_CN.md` for the export and validation checklist.

CQ-1 Material Class/Data v1 validation uses the generated private asset
`laevat_skinned/laevat_skinned_material_cq1_v2.glb`. It embeds versioned
`extras.azureRenderMaterial` profiles and remains excluded from version
control. See `docs/MATERIAL_SYSTEM_V1_CN.md` for its schema and audit.
