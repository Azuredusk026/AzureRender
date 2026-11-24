"""Stage the Recovery desktop loading loop as project-relative RmlUi sprites."""

import argparse
from collections import defaultdict
import json
import math
import os
from pathlib import Path
import shutil
import tempfile

from tools.npr_room.catalog_storage import publish_directory
from tools.npr_room.pbr_materials import write
from tools.npr_room.reference_inventory import digest
from tools.npr_room.skinned_mesh_export import UnityScene


BOOT = "Assets/App/Scenes/Boot.unity"
LOOP = "Assets/AnimationClip/anim_ui_loading.anim"
CLIP_ID = 7400000
DESKTOP_CANVAS = 66
STATUS = "Loading room. Press Backspace to cancel."


def sample_curve(keys, time, stepped=False):
    if time <= keys[0]["time"]:
        return keys[0]["value"]
    for left, right in zip(keys, keys[1:]):
        if time >= right["time"]:
            continue
        if stepped:
            return left["value"]
        if left["weightedMode"] or right["weightedMode"]:
            raise ValueError("Weighted Unity curves require an editor sample")
        outgoing, incoming = float(left["outSlope"]), float(right["inSlope"])
        if not math.isfinite(outgoing) or not math.isfinite(incoming):
            return left["value"]
        duration = right["time"] - left["time"]
        t = (time - left["time"]) / duration
        return ((2*t**3 - 3*t**2 + 1) * left["value"] + (t**3 - 2*t**2 + t) * outgoing * duration
                + (-2*t**3 + 3*t**2) * right["value"] + (t**3 - t**2) * incoming * duration)
    return keys[-1]["value"]


def desktop_elements(scene):
    canvas = scene.read(DESKTOP_CANVAS)
    if scene.read(canvas["m_GameObject"]["fileID"])["m_Name"] != "LoadingCanvas":
        raise ValueError("Expected the original desktop loading canvas")
    root, = canvas["m_Children"]
    root = scene.read(root["fileID"])
    result = {}

    def visit(transform_id, prefix):
        transform = scene.read(transform_id)
        game = scene.read(transform["m_GameObject"]["fileID"])
        name = prefix + game["m_Name"]
        images = []
        for component in game["m_Component"]:
            identity = component["component"]["fileID"]
            if scene.documents[identity][0] != 114:
                continue
            properties = scene.read(identity)
            if "m_Sprite" in properties:
                images.append(properties)
        if len(images) > 1 or name in result or not game["m_IsActive"]:
            raise ValueError(f"Unexpected desktop loading element: {name}")
        result[name] = (transform, images[0] if images else None)
        for item in transform["m_Children"]:
            visit(item["fileID"], name + "/")

    for item in root["m_Children"]:
        visit(item["fileID"], "")
    return result


def curves_in(clip, elements):
    curves = defaultdict(dict)
    for row in clip["m_FloatCurves"]:
        path, prop = row["path"], row["attribute"]
        if path in elements:
            curves[path][prop] = row["curve"]["m_Curve"]
    for row in clip["m_EulerCurves"]:
        path = row["path"]
        if path in elements:
            curves[path]["rotation.z"] = [
                {**key, **{name: key[name]["z"] for name in ("value", "inSlope", "outSlope")}}
                for key in row["curve"]["m_Curve"]
            ]
    return curves


def stage_document(elements, clip, sprite_catalog, image_root, output):
    images = json.loads((image_root / "image-catalog.json").read_bytes())["slots"]
    curves = curves_in(clip, elements)
    source_settings = clip["m_AnimationClipSettings"]
    duration = source_settings["m_StopTime"]
    rate = clip["m_SampleRate"]
    if source_settings["m_StartTime"] or source_settings["m_LoopTime"] != 1 or rate != 60 or duration <= 0:
        raise ValueError("Expected a loop starting at zero with 60 Hz samples")
    frames = round(rate * duration)
    styles = ["body { width:100%; height:100%; margin:0; background-color:#000; color:white; font-family:LatoLatin; }",
              "#transition-status { position:absolute; left:24dp; bottom:24dp; font-size:18dp; }"]
    sprite_records = {}

    def draw(path):
        transform, image = elements[path]
        uid = "splash-" + path.lower().replace("/", "-")
        position = transform["m_AnchoredPosition"]
        size = transform["m_SizeDelta"]
        anchor = transform["m_AnchorMin"]
        target = image["m_Sprite"] if image else {}
        background = path == "Image"
        css = "position:absolute;"
        if background:
            css += "left:0;top:0;right:0;bottom:0;"
            color = image["m_Color"]
            css += "background-color:#" + "".join(f"{round(color[channel] * 255):02x}" for channel in "rgb") + ";"
        elif target.get("fileID"):
            css += (f"left:{100*anchor['x']:g}%;top:{100*(1-anchor['y']):g}%;"
                    f"width:{100*size['x']/1920:.9g}vw;height:{100*size['y']/1920:.9g}vw;"
                    f"margin-left:{-50*size['x']/1920:.9g}vw;margin-top:{-50*size['y']/1920:.9g}vw;")
        else:
            css += "left:50%;top:50%;width:0;height:0;"

        track = curves[path]

        def key(prop, time, default, stepped=False):
            values = track.get(prop)
            return sample_curve(values, time, stepped) if values else default

        def style(time):
            alpha = image["m_Color"]["a"] if image else 1
            opacity = key("m_Color.a", time, alpha) * key("m_IsActive", time, 1, True)
            result = f"opacity:{max(0, min(1, opacity)):.9g};"
            if not background:
                x = 100 * position["x"] / 1920
                y = -100 * key("m_AnchoredPosition.y", time, position["y"]) / 1920
                rotation = -key("rotation.z", time, 0)
                result += f"transform:translate({x:g}vw, {y:.9g}vw) rotate({rotation:.9g}deg);"
            return result

        if track:
            css += f"animation:{duration}s linear-in-out infinite {uid};"
            steps = [f"{100*i/frames:.9g}% {{ {style(i/rate)} }}" for i in range(frames + 1)]
            styles.append(f"@keyframes {uid} {{ " + " ".join(steps) + " }")
        styles.append(f"#{uid} {{ {css} {style(0)} }}")

        if target.get("fileID"):
            sprite = sprite_catalog[target["guid"]]
            resource = images[sprite["texture"]]
            rectangle = sprite["textureRectangle"]
            if (sprite["fileID"] != target["fileID"] or sprite["alphaTexture"] is not None
                    or [rectangle[name] for name in ("x", "y", "width", "height")]
                    != [0, 0, resource["width"], resource["height"]]):
                raise ValueError(f"Loading sprite requires a texture subregion: {path}")
            texture = (image_root / resource["media"]).resolve(strict=True)
            if not texture.is_relative_to(image_root.resolve()) or digest(texture) != resource["sha256"]:
                raise ValueError(f"Loading sprite texture differs from the locked source: {path}")
            filename = uid + ".png"
            os.link(texture, output / filename)
            sprite_records[path] = {"source": sprite["source"], "asset": filename, "sha256": resource["sha256"]}
            return f'<img id="{uid}" src="{filename}" />'

        children = [draw(name) for name in elements if name.rpartition("/")[0] == path]
        return f'<div id="{uid}">' + "".join(children) + "</div>"

    body = [draw(name) for name in elements if "/" not in name]
    body.append(f'<div id="transition-status">{STATUS}</div>')
    document = "<rml><head><title>Room loading</title><style>\n" + "\n".join(styles)
    document += "\n</style></head><body>\n" + "\n".join(body) + "\n</body></rml>\n"
    return document, {"sprites": sprite_records, "sampleRate": rate, "frameCount": frames + 1,
                      "duration": duration, "curveCount": sum(map(len, curves.values()))}


def stage_project(source, catalog, room, output):
    boot = source / BOOT
    clip_path = source / LOOP
    elements = desktop_elements(UnityScene(boot))
    clip = UnityScene(clip_path).read(CLIP_ID)
    sprites = json.loads((catalog / "source-lock/sprite-catalog.json").read_bytes())["slots"]

    def copy_asset(path, destination):
        return os.link(path, destination) if Path(path).suffix.lower() == ".glb" else shutil.copyfile(path, destination)

    with tempfile.TemporaryDirectory(prefix="startup-splash-", dir=output.parent) as holder:
        project = Path(holder) / "project"
        project.mkdir()
        shutil.copyfile(room / "project.azureproject", project / "project.azureproject")
        assets = project / "assets"
        shutil.copytree(room / "assets", assets, copy_function=copy_asset)
        document, report = stage_document(elements, clip, sprites, catalog / "image-catalog", assets)
        rml = assets / "startup-flow.rml"
        rml.write_text(document, encoding="utf-8", newline="\n")
        for name in ("boot", "asset-load", "entry", "transition"):
            level_path = assets / (name + ".azurelevel")
            level = json.loads(level_path.read_bytes())
            node, = level["nodes"]
            node["components"]["azure.game-ui"] = {
                "type": "azure.game-ui", "version": 1, "data": {"asset": "assets:/startup-flow.rml", "enabled": True},
            }
            write(level_path, level)
        report.update({"formatVersion": 1, "sceneSHA256": digest(boot), "clipSHA256": digest(clip_path),
                       "documentSHA256": digest(rml)})
        write(project / "splash-receipt.json", report)
        publish_directory(project, output)
    return report


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    for arg in ("source", "catalog", "room", "output"):
        parser.add_argument("--" + arg, type=Path, required=True)
    args = parser.parse_args()
    print(json.dumps(stage_project(args.source, args.catalog, args.room, args.output), sort_keys=True))


if __name__ == "__main__":
    main()
