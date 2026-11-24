"""Check the installed game's weather shader and room identities against the frozen reference."""

import argparse
from hashlib import sha256
import json
from pathlib import Path

from tools.npr_room.reference_inventory import digest
from tools.npr_room.unity_d3d11_programs import ATMOSPHERE


def inspect(data):
    import UnityPy

    managers = UnityPy.load(str(data / "globalgamemanagers")).objects
    player, = (obj.read_typetree() for obj in managers if obj.type.name == "PlayerSettings")
    build, = (obj.read_typetree() for obj in managers if obj.type.name == "BuildSettings")
    room_index, = (index for index, scene in enumerate(build["scenes"])
                   if Path(scene).name == "RoomScene.unity")
    room = f"level{room_index}"
    resources = data / "resources.assets"
    names = {name: role for role, name in ATMOSPHERE.items()}
    weather = {}
    for obj in UnityPy.load(str(resources)).objects:
        if obj.type.name != "Shader":
            continue
        name = obj.read_typetree()["m_ParsedForm"]["m_Name"]
        if name not in names:
            continue
        role = names[name]
        if role in weather:
            raise ValueError(f"Duplicate source shader: {name}")
        weather[role] = {"name": name, "pathId": obj.path_id, "objectSHA256": sha256(obj.get_raw_data()).hexdigest()}
    if weather.keys() != ATMOSPHERE.keys():
        raise ValueError(f"Missing weather shaders: {sorted(ATMOSPHERE.keys() - weather.keys())}")
    return {
        "version": player["bundleVersion"],
        "unityVersion": build["m_Version"],
        "scenes": build["scenes"],
        "resourcesSHA256": digest(resources),
        "room": {"file": room, "bytes": (data / room).stat().st_size, "sha256": digest(data / room)},
        "gameAssemblySHA256": digest(data / "Managed/Assembly-CSharp.dll"),
        "weather": weather,
    }


def report(reference, installed, program_receipt):
    baseline, current = inspect(reference), inspect(installed)
    known = json.loads(program_receipt.read_bytes())
    if baseline["resourcesSHA256"] != known["sourceSHA256"]:
        raise ValueError("Reference resources do not match the archived D3D11 program receipt")
    shaders = {}
    for role, original in baseline["weather"].items():
        source = known["programs"][role]
        if (original["name"], original["pathId"], original["objectSHA256"]) != (
                source["shader"], source["sourcePathId"], source["sourceObjectSHA256"]):
            raise ValueError(f"Reference shader does not match the program receipt: {role}")
        newer = current["weather"][role]
        shaders[role] = {"reference": original, "installed": newer,
                         "samePathId": original["pathId"] == newer["pathId"],
                         "sameObject": original["objectSHA256"] == newer["objectSHA256"]}
    del baseline["weather"], current["weather"]
    return {"schemaVersion": 1, "programReceiptSHA256": digest(program_receipt),
            "reference": baseline, "installed": current, "shaders": shaders,
            "sameSceneOrder": baseline["scenes"] == current["scenes"],
            "sameRoomFile": baseline["room"]["sha256"] == current["room"]["sha256"],
            "sameGameAssembly": baseline["gameAssemblySHA256"] == current["gameAssemblySHA256"]}


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    for name in ("reference", "installed", "program-receipt", "output"):
        parser.add_argument("--" + name, type=Path, required=True)
    args = parser.parse_args()
    evidence = report(args.reference, args.installed, args.program_receipt)
    with args.output.open("x", encoding="utf-8", newline="\n") as output:
        json.dump(evidence, output, indent=2, sort_keys=True)
        output.write("\n")


if __name__ == "__main__":
    main()
