"""Expose an embedded GLB payload as a relative glTF external buffer."""

import json
from struct import unpack
from urllib.parse import quote


def reference_document(source):
    if quote(source.name, safe="") != source.name:
        raise ValueError("Referenced model filename must be URI-safe for the asset database")
    size = source.stat().st_size
    with source.open("rb") as stream:
        header = stream.read(20)
        if len(header) != 20:
            raise ValueError("Truncated GLB header")
        magic, version, total, json_size, kind = unpack("<5I", header)
        if (magic, version, total, kind) != (0x46546C67, 2, size, 0x4E4F534A):
            raise ValueError("Invalid GLB container")
        if json_size % 4 or json_size > size - 28:
            raise ValueError("Invalid GLB JSON length")
        document = json.loads(stream.read(json_size))
        binary_size, binary_kind = unpack("<2I", stream.read(8))
        offset = stream.tell()
        if binary_kind != 0x004E4942 or binary_size % 4 or offset + binary_size != size:
            raise ValueError("Invalid GLB binary chunk")
    buffers = document.get("buffers", [])
    if len(buffers) > 1 or any("uri" in buffer for buffer in buffers):
        raise ValueError("Referenced model requires one embedded buffer")
    declared = buffers[0]["byteLength"] if buffers else 0
    if type(declared) is not int or declared < 0 or not 0 <= binary_size - declared <= 3:
        raise ValueError("Invalid embedded buffer length")
    for image in document.get("images", []):
        if "uri" in image and not image["uri"].startswith("data:"):
            raise ValueError("Referenced model requires embedded images")
    for view in document.get("bufferViews", []):
        start, length = view.get("byteOffset", 0), view["byteLength"]
        if (view.get("buffer") != 0 or type(start) is not int or type(length) is not int
                or start < 0 or length < 0 or start + length > declared):
            raise ValueError("Invalid embedded buffer view")
        if "EXT_meshopt_compression" in view.get("extensions", {}):
            raise ValueError("Compressed external buffer offsets require explicit conversion")
        view["byteOffset"] = offset + start
    document["buffers"] = [{"uri": quote(source.name, safe=""), "byteLength": size}]
    return document
