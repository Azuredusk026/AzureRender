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
SKELETON_PATH = "/Game/ZMD/莱万汀/莱万汀_Skeleton"
OUTPUT_PATH = os.path.join(
    PROJECT_ROOT,
    "assets_private",
    "laevat_skinned",
    "compatible_animations.json",
)


def main():
    target_skeleton = unreal.load_asset(SKELETON_PATH)
    if target_skeleton is None:
        raise RuntimeError(f"Unable to load skeleton: {SKELETON_PATH}")

    registry = unreal.AssetRegistryHelpers.get_asset_registry()
    registry.search_all_assets(True)
    compatible = []
    animation_assets = 0
    for asset_data in registry.get_all_assets():
        if "AnimSequence" not in str(asset_data.asset_class_path):
            continue
        animation_assets += 1
        animation = asset_data.get_asset()
        if animation is None:
            continue
        skeleton = animation.get_editor_property("skeleton")
        if skeleton is None or skeleton.get_path_name() != target_skeleton.get_path_name():
            continue
        compatible.append(
            {
                "name": animation.get_name(),
                "path": animation.get_path_name(),
                "sequence_length": animation.get_editor_property(
                    "sequence_length"
                ),
            }
        )

    os.makedirs(os.path.dirname(OUTPUT_PATH), exist_ok=True)
    with open(OUTPUT_PATH, "w", encoding="utf-8") as output:
        json.dump(
            {
                "target_skeleton": target_skeleton.get_path_name(),
                "animation_assets_scanned": animation_assets,
                "compatible_animations": compatible,
            },
            output,
            ensure_ascii=False,
            indent=2,
        )
    unreal.log(
        f"LAEVAT_ANIMATION_SCAN_SUCCESS animations={animation_assets} "
        f"compatible={len(compatible)} output={OUTPUT_PATH}"
    )


main()
