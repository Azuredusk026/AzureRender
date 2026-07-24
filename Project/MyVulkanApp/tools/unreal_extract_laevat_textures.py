import json
import os

import unreal


ASSET_ROOT = "/Game/ZMD/莱万汀"
MATERIAL_ROOT = ASSET_ROOT + "/MI"
TEXTURE_ROOT = ASSET_ROOT + "/Tex"
OUTPUT_ROOT = (
    r"D:\Assigment\2609\FYP\Project\MyVulkanApp"
    r"\assets_private\laevat_static\textures"
)
MANIFEST_PATH = (
    r"D:\Assigment\2609\FYP\Project\MyVulkanApp"
    r"\assets_private\laevat_static\unreal_material_textures.json"
)


def list_assets(path):
    return list(
        unreal.EditorAssetLibrary.list_assets(
            path, recursive=True, include_folder=False
        )
    )


def export_texture(texture, output_path):
    task = unreal.AssetExportTask()
    task.object = texture
    task.filename = output_path
    task.automated = True
    task.prompt = False
    task.replace_identical = True
    task.use_file_archive = False
    task.write_empty_files = False
    task.exporter = unreal.TextureExporterPNG()
    return bool(unreal.Exporter.run_asset_export_task(task))


def get_instance_texture_parameters(material):
    parameters = {}
    current = material
    visited = set()

    while current is not None and current.get_path_name() not in visited:
        visited.add(current.get_path_name())
        try:
            values = current.get_editor_property("texture_parameter_values")
        except Exception:
            values = []

        for value in values:
            info = value.get_editor_property("parameter_info")
            texture = value.get_editor_property("parameter_value")
            if texture is not None:
                parameters.setdefault(
                    str(info.get_editor_property("name")), texture.get_path_name()
                )

        try:
            current = current.get_editor_property("parent")
        except Exception:
            current = None

    return parameters


def get_parent_chain(material):
    parents = []
    current = material
    visited = set()
    while current is not None and current.get_path_name() not in visited:
        visited.add(current.get_path_name())
        parents.append(current.get_path_name())
        try:
            current = current.get_editor_property("parent")
        except Exception:
            current = None
    return parents


def get_instance_parameters(material, property_name, value_converter):
    parameters = {}
    current = material
    visited = set()
    while current is not None and current.get_path_name() not in visited:
        visited.add(current.get_path_name())
        try:
            values = current.get_editor_property(property_name)
        except Exception:
            values = []

        for value in values:
            info = value.get_editor_property("parameter_info")
            parameter_value = value.get_editor_property("parameter_value")
            parameters.setdefault(
                str(info.get_editor_property("name")),
                value_converter(parameter_value),
            )

        try:
            current = current.get_editor_property("parent")
        except Exception:
            current = None
    return parameters


def color_to_dict(color):
    return {
        "r": color.r,
        "g": color.g,
        "b": color.b,
        "a": color.a,
    }


def main():
    os.makedirs(OUTPUT_ROOT, exist_ok=True)

    exported = {}
    for asset_path in list_assets(TEXTURE_ROOT):
        asset = unreal.EditorAssetLibrary.load_asset(asset_path)
        if not isinstance(asset, unreal.Texture2D):
            continue

        name = asset.get_name()
        output_path = os.path.join(OUTPUT_ROOT, name + ".png")
        if export_texture(asset, output_path):
            exported[asset_path] = output_path
            unreal.log("LAEVAT_TEXTURE_EXPORTED " + output_path)
        else:
            unreal.log_warning("LAEVAT_TEXTURE_EXPORT_FAILED " + asset_path)

    material_textures = {}
    material_details = {}
    for asset_path in list_assets(MATERIAL_ROOT):
        material = unreal.EditorAssetLibrary.load_asset(asset_path)
        if not isinstance(material, unreal.MaterialInterface):
            continue

        material_textures[material.get_name()] = get_instance_texture_parameters(
            material
        )
        material_details[material.get_name()] = {
            "parent_chain": get_parent_chain(material),
            "scalar_parameters": get_instance_parameters(
                material, "scalar_parameter_values", float
            ),
            "vector_parameters": get_instance_parameters(
                material, "vector_parameter_values", color_to_dict
            ),
        }
        unreal.log(
            "LAEVAT_MATERIAL_TEXTURES "
            + material.get_name()
            + " "
            + json.dumps(
                material_textures[material.get_name()], ensure_ascii=False
            )
        )

    manifest = {
        "asset_root": ASSET_ROOT,
        "exported_textures": exported,
        "material_textures": material_textures,
        "material_details": material_details,
    }
    with open(MANIFEST_PATH, "w", encoding="utf-8") as file:
        json.dump(manifest, file, ensure_ascii=False, indent=2)

    unreal.log(
        "LAEVAT_TEXTURE_EXTRACTION_SUCCESS "
        f"textures={len(exported)} materials={len(material_textures)} "
        f"manifest={MANIFEST_PATH}"
    )


main()
