import json
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
MATERIAL_PATH = "/Game/ZMD/MaterialLibrary/M_Common_Cloth"
OUTPUT_PATH = os.path.join(
    PROJECT_ROOT,
    "assets_private",
    "laevat_static",
    "cloth_material_graph.json",
)


def safe_property(obj, name):
    try:
        value = obj.get_editor_property(name)
    except Exception:
        return None
    if isinstance(value, unreal.Object):
        return value.get_path_name()
    if isinstance(value, (str, int, float, bool)) or value is None:
        return value
    return str(value)


def main():
    material = unreal.EditorAssetLibrary.load_asset(MATERIAL_PATH)
    if material is None:
        raise RuntimeError("Could not load " + MATERIAL_PATH)

    property_names = (
        "MP_BASE_COLOR",
        "MP_METALLIC",
        "MP_SPECULAR",
        "MP_ROUGHNESS",
        "MP_AMBIENT_OCCLUSION",
        "MP_EMISSIVE_COLOR",
        "MP_NORMAL",
        "MP_OPACITY",
        "MP_OPACITY_MASK",
    )
    roots = {}
    pending = []
    for property_name in property_names:
        material_property = getattr(unreal.MaterialProperty, property_name, None)
        if material_property is None:
            continue
        expression = unreal.MaterialEditingLibrary.get_material_property_input_node(
            material, material_property
        )
        if expression is not None:
            roots[property_name] = {
                "expression": expression.get_name(),
                "output": str(
                    unreal.MaterialEditingLibrary
                    .get_material_property_input_node_output_name(
                        material, material_property
                    )
                ),
            }
            pending.append(expression)

    expressions = []
    visited = set()
    while pending:
        expression = pending.pop()
        path_name = expression.get_path_name()
        if path_name in visited:
            continue
        visited.add(path_name)
        expressions.append(expression)
        try:
            pending.extend(
                unreal.MaterialEditingLibrary.get_inputs_for_material_expression(
                    expression
                )
            )
        except Exception:
            pass
    output = []
    interesting_properties = (
        "desc",
        "parameter_name",
        "texture",
        "r",
        "g",
        "b",
        "a",
        "const_input",
        "default_value",
        "channel_names",
    )
    input_properties = (
        "input",
        "a",
        "b",
        "alpha",
        "coordinate",
        "texture_object",
        "base",
        "exponent",
        "value",
    )

    for expression in expressions:
        item = {
            "name": expression.get_name(),
            "class": expression.get_class().get_name(),
        }
        for property_name in interesting_properties:
            value = safe_property(expression, property_name)
            if value is not None:
                item[property_name] = value
        inputs = {}
        for property_name in input_properties:
            try:
                material_input = expression.get_editor_property(property_name)
                source = material_input.get_editor_property("expression")
                if source is not None:
                    inputs[property_name] = {
                        "expression": source.get_name(),
                        "output_index": material_input.get_editor_property(
                            "output_index"
                        ),
                    }
            except Exception:
                pass
        if inputs:
            item["inputs"] = inputs
        try:
            connected_inputs = (
                unreal.MaterialEditingLibrary.get_inputs_for_material_expression(
                    expression
                )
            )
            if connected_inputs:
                item["connected_inputs"] = [
                    source.get_name() for source in connected_inputs
                ]
        except Exception:
            pass
        output.append(item)

    with open(OUTPUT_PATH, "w", encoding="utf-8") as file:
        json.dump(
            {
                "material": material.get_path_name(),
                "roots": roots,
                "expression_count": len(output),
                "expressions": output,
            },
            file,
            ensure_ascii=False,
            indent=2,
        )
    unreal.log(
        "LAEVAT_CLOTH_GRAPH_SUCCESS "
        f"expressions={len(output)} output={OUTPUT_PATH}"
    )


main()
