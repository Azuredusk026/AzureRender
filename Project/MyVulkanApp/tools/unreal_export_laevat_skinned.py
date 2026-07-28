import os
import unreal


ASSET_PATH = "/Game/ZMD/莱万汀/莱万汀"
OUTPUT_PATH = os.path.normpath(
    r"D:\Assigment\2609\FYP\Project\MyVulkanApp"
    r"\assets_private\laevat_skinned\laevat_skinned.glb"
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
