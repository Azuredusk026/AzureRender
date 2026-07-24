import os

import unreal


MATCAP_ASSET_PATH = "/Game/Matcap/Matcap01"
OUTPUT_PATH = (
    r"D:\Assigment\2609\FYP\Project\MyVulkanApp"
    r"\assets_private\laevat_static\textures\Matcap01.png"
)


def main():
    texture = unreal.EditorAssetLibrary.load_asset(MATCAP_ASSET_PATH)
    if not isinstance(texture, unreal.Texture2D):
        raise RuntimeError(
            f"Expected Texture2D at {MATCAP_ASSET_PATH}, got {type(texture)}"
        )

    os.makedirs(os.path.dirname(OUTPUT_PATH), exist_ok=True)
    task = unreal.AssetExportTask()
    task.object = texture
    task.filename = OUTPUT_PATH
    task.automated = True
    task.prompt = False
    task.replace_identical = True
    task.use_file_archive = False
    task.write_empty_files = False
    task.exporter = unreal.TextureExporterPNG()

    if not unreal.Exporter.run_asset_export_task(task):
        raise RuntimeError(f"Failed to export {MATCAP_ASSET_PATH}")

    unreal.log(
        "LAEVAT_MATCAP_EXPORT_SUCCESS "
        f"asset={MATCAP_ASSET_PATH} output={OUTPUT_PATH}"
    )


main()
