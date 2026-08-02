import os
import unreal


def resolve_project_root():
    override = os.environ.get("AZURERENDER_PROJECT_ROOT")
    if override:
        return os.path.abspath(os.path.expanduser(os.path.expandvars(override)))
    script_file = globals().get("__file__")
    if script_file:
        return os.path.dirname(os.path.dirname(os.path.abspath(script_file)))
    raise RuntimeError(
        "Set AZURERENDER_PROJECT_ROOT when Unreal does not provide __file__"
    )


PROJECT_ROOT = resolve_project_root()
ASSET_PATH = "/Game/ZMD/莱万汀/莱万汀"
OUTPUT_PATH = os.path.join(
    PROJECT_ROOT,
    "assets_private",
    "laevat_skinned",
    "laevat_skinned.glb",
)


def main():
    os.makedirs(os.path.dirname(OUTPUT_PATH), exist_ok=True)
    asset = unreal.load_asset(ASSET_PATH)
    if asset is None:
        raise RuntimeError(f"Unable to load Unreal asset: {ASSET_PATH}")

    unreal.log(f"Skinned export source: {asset.get_path_name()}")
    unreal.log(f"Skinned export class: {asset.get_class().get_name()}")

    options = unreal.GLTFExportOptions()
    options.export_uniform_scale = 0.01
    options.export_vertex_colors = False
    options.export_vertex_skin_weights = True
    options.export_morph_targets = False
    options.export_animation_sequences = False
    options.bake_material_inputs = unreal.GLTFMaterialBakeMode.USE_MESH_DATA
    options.texture_image_format = unreal.GLTFTextureImageFormat.PNG
    options.adjust_normalmaps = True
    options.export_texture_transforms = True

    success = unreal.GLTFExporter.export_to_gltf(
        asset,
        OUTPUT_PATH,
        options,
        set(),
    )
    if not success:
        raise RuntimeError("Unreal glTF skinned export returned failure")
    if not os.path.isfile(OUTPUT_PATH):
        raise RuntimeError(f"Exporter did not create output: {OUTPUT_PATH}")

    unreal.log(
        f"LAEVAT_SKINNED_EXPORT_SUCCESS path={OUTPUT_PATH} "
        f"bytes={os.path.getsize(OUTPUT_PATH)}"
    )


main()
