"""Describe and validate source-locked atmosphere program global metadata."""

import json
import math
from tools.npr_room.reference_inventory import digest

CAMERA_FIELDS = {"_WorldSpaceCameraPos", "_ProjectionParams"}


def make_bindings(programs):
    document = json.loads(programs.read_bytes())
    widths = {}
    for program in document["programs"].values():
        for stage in program["stages"].values():
            for buffer in stage["constantBuffers"]:
                if buffer["name"] != "$Globals":
                    continue
                for field in buffer["vectors"]:
                    name, width = field["name"], field["dimension"]
                    if name in CAMERA_FIELDS:
                        continue
                    if (type(width) is not int or not 1 <= width <= 4
                            or field["array"] != 0 or field["type"] != 0):
                        raise ValueError("Program globals require scalar or vector float bindings")
                    if name in widths and widths[name] != width:
                        raise ValueError("Conflicting program global width: " + name)
                    widths[name] = width
    if not 0 < len(widths) <= 256:
        raise ValueError("Program global binding count is outside the capture budget")
    return {"schemaVersion": 1, "programSHA256": digest(programs),
            "globals": [{"name": name, "dimension": widths[name]} for name in sorted(widths)]}


def verify_metadata(record, definition):
    if record.get("programManifestSHA256") != definition["programSHA256"]:
        raise ValueError("Program metadata requires its declared source program fingerprint")
    expected = {row["name"]: row["dimension"] for row in definition["globals"]}
    rows = record.get("programGlobals", [])
    fields = {row["name"]: row for row in rows}
    if len(fields) != len(rows) or fields.keys() != expected.keys():
        raise ValueError("Captured program global bindings are incomplete or duplicated")
    for name, width in expected.items():
        row = fields[name]
        if row["dimension"] != width or not all(math.isfinite(row["value"][axis]) for axis in "xyzw"):
            raise ValueError("Captured program global width or value is invalid")
    time = record["simulationTime"]
    if not math.isfinite(time):
        raise ValueError("Captured program time must be finite")
    for name, axis in [("_TimeParameters", "x"), ("_Time", "y")]:
        if name in fields and not math.isclose(fields[name]["value"][axis], time, rel_tol=0, abs_tol=1e-4):
            raise ValueError("Program globals differ from the recorded simulation frame")
    return {"globals": len(fields), "programSHA256": definition["programSHA256"]}
