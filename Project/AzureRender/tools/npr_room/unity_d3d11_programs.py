"""Extract original Windows shader programs and binding tables."""

import argparse
import ctypes
from hashlib import sha256
import json
from pathlib import Path
import re
from struct import unpack_from
import tempfile

from tools.npr_room.catalog_storage import publish_directory
from tools.npr_room.reference_inventory import digest


ATMOSPHERE = {
    "sky": "Distant Lands/Cozy/URP/Stylized Sky Desktop",
    "cozy-cloud": "Distant Lands/Cozy/URP/Stylized Clouds (COZY Desktop)",
    "ghibli-cloud": "Distant Lands/Cozy/URP/Stylized Clouds (Ghibli Desktop)",
    "fog": "Distant Lands/Cozy/URP/Stylized Fog (Desktop)",
    "moon": "Distant Lands/Cozy/URP/Stylized Moon",
}

OPENING_EFFECTS = {
    "chromatic": "Shader Graphs/ScreenColorGlitch",
    "blackout": "Shader Graphs/ScreenBlackout",
    "noise": "Shader Graphs/NoiseShader",
    "screen-move": "Shader Graphs/ScreenMoveGlitch",
    "glitch": "Shader Graphs/ScreenGlitch",
}


def dxbc_entry(block, index):
    if len(block) < 4:
        raise ValueError("Unity shader program table is truncated")
    count, = unpack_from("<I", block)
    if not 0 <= index < count or 4 + count * 12 > len(block):
        raise ValueError(f"Unity shader program index is out of range: {index}")
    start, length, segment = unpack_from("<III", block, 4 + index * 12)
    if segment != 0 or start < 4 + count * 12 or start + length > len(block):
        raise ValueError(f"Unity shader program range is invalid: {index}")
    entry = block[start:start + length]
    candidates = []
    for match in re.finditer(b"DXBC", entry):
        offset = match.start()
        if offset + 32 > len(entry):
            continue
        version, size, chunks = unpack_from("<III", entry, offset + 20)
        if version != 1 or chunks == 0 or 32 + chunks * 4 > size or offset + size > len(entry):
            continue
        pieces = unpack_from(f"<{chunks}I", entry, offset + 32)
        if any(piece < 32 + chunks * 4 or piece + 8 > size for piece in pieces):
            continue
        if any(piece + 8 + unpack_from("<I", entry, offset + piece + 4)[0] > size for piece in pieces):
            continue
        candidates.append(entry[offset:offset + size])
    if len(candidates) != 1:
        raise ValueError(f"Expected one DXBC shader in Unity program entry {index}")
    return candidates[0]


def program_bindings(parameters, names):
    def element(row, dimension):
        return {"name": names[row["m_NameIndex"]], "offset": row["m_Index"], "type": row["m_Type"],
                "array": row["m_ArraySize"], "dimension": row[dimension]}

    locations = {row["m_NameIndex"]: row["m_Index"] for row in parameters["m_ConstantBufferBindings"]}
    buffers = [{"name": names[row["m_NameIndex"]], "binding": locations[row["m_NameIndex"]], "bytes": row["m_Size"],
                "vectors": [element(field, "m_Dim") for field in row["m_VectorParams"]],
                "matrices": [element(field, "m_RowCount") for field in row["m_MatrixParams"]]}
               for row in parameters["m_ConstantBuffers"]]
    textures = [{"name": names[row["m_NameIndex"]], "binding": row["m_Index"], "sampler": row["m_SamplerIndex"],
                 "dimension": row["m_Dim"], "multisampled": row["m_MultiSampled"]}
                for row in parameters["m_TextureParams"]]
    return {"constantBuffers": buffers, "textures": textures}


def disassemble(program):
    library = ctypes.WinDLL("d3dcompiler_47.dll")
    library.D3DDisassemble.argtypes = [ctypes.c_void_p, ctypes.c_size_t, ctypes.c_uint, ctypes.c_char_p,
                                       ctypes.POINTER(ctypes.c_void_p)]
    library.D3DDisassemble.restype = ctypes.c_long
    blob = ctypes.c_void_p()
    status = library.D3DDisassemble(program, len(program), 0, None, ctypes.byref(blob))
    if status != 0:
        raise ValueError(f"Direct3D could not decode the original shader: HRESULT {status & 0xFFFFFFFF:#x}")
    vtable = ctypes.cast(blob, ctypes.POINTER(ctypes.POINTER(ctypes.c_void_p))).contents
    try:
        pointer = ctypes.WINFUNCTYPE(ctypes.c_void_p, ctypes.c_void_p)(vtable[3])(blob)
        length = ctypes.WINFUNCTYPE(ctypes.c_size_t, ctypes.c_void_p)(vtable[4])(blob)
        return ctypes.string_at(pointer, length).decode("utf-8").rstrip("\0")
    finally:
        ctypes.WINFUNCTYPE(ctypes.c_ulong, ctypes.c_void_p)(vtable[2])(blob)


def extract(source, manifest, target, shaders=ATMOSPHERE):
    # The source archive and its shader inventory are local, private inputs.
    import lz4.block
    import UnityPy

    locked = {row["name"]: row for row in json.loads(manifest.read_bytes()) if row["source"] == source.name}
    payloads = {obj.path_id: obj for obj in UnityPy.load(str(source)).objects if obj.type.name == "Shader"}
    target.mkdir()
    programs = {}
    for role, name in shaders.items():
        original = locked[name]
        obj = payloads[original["sourcePathId"]]
        shader = obj.read_typetree()
        source_hash = sha256(obj.get_raw_data()).hexdigest()
        if name != shader["m_ParsedForm"]["m_Name"] or source_hash != original["sourceRawSha256"]:
            raise ValueError(f"Original shader identity differs from the locked Recovery object: {name}")
        direct3d = shader["platforms"].index(4)
        offset, = shader["offsets"][direct3d]
        compression, = shader["compressedLengths"][direct3d]
        length, = shader["decompressedLengths"][direct3d]
        data = lz4.block.decompress(bytes(shader["compressedBlob"])[offset:offset + compression], uncompressed_size=length)
        first = shader["m_ParsedForm"]["m_SubShaders"][0]["m_Passes"][0]
        names = {index: value for value, index in first["m_NameIndices"]}
        stages = {}
        for stage in ("Vertex", "Fragment"):
            details = first["prog" + stage]
            active = [index for index, variants in enumerate(details["m_PlayerSubPrograms"]) if variants]
            platform_row, = active
            variant, = details["m_PlayerSubPrograms"][platform_row]
            parameters, = details["m_ParameterBlobIndices"][platform_row]
            program = dxbc_entry(data, variant["m_BlobIndex"])
            native = disassemble(program)
            if not re.search(r"(?m)^" + ("vs" if stage == "Vertex" else "ps") + r"_\d_\d\s*$", native):
                raise ValueError(f"Windows shader stage differs from the archived Unity binding: {name}/{stage}")
            filename = f"{role}-{stage.lower()}"
            (target / (filename + ".dxbc")).write_bytes(program)
            (target / (filename + ".txt")).write_text(native, encoding="utf-8", newline="\n")
            stages[stage.lower()] = {"path": filename + ".dxbc", "sha256": sha256(program).hexdigest(),
                                     "sourceProgram": variant["m_BlobIndex"], "parameterBlob": parameters,
                                     **program_bindings(details["m_CommonParameters"], names)}
        programs[role] = {"shader": name, "sourcePathId": obj.path_id, "sourceObjectSHA256": source_hash,
                          "pass": 0, "stages": stages}
    return {"schemaVersion": 1, "sourceSHA256": digest(source), "manifestSHA256": digest(manifest), "programs": programs}


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    for option in ("source", "manifest", "output"):
        parser.add_argument("--" + option, type=Path, required=True)
    families = {"atmosphere": ATMOSPHERE, "opening-effects": OPENING_EFFECTS}
    parser.add_argument("--family", choices=families, default="atmosphere")
    args = parser.parse_args()
    if args.output.exists():
        raise FileExistsError(args.output)
    with tempfile.TemporaryDirectory(prefix="d3d11-programs-", dir=args.output.parent) as container:
        staged = Path(container) / "programs"
        data = extract(args.source, args.manifest, staged, families[args.family])
        name = f"windows-{args.family}-programs.json"
        (staged / name).write_text(json.dumps(data, sort_keys=True, separators=(",", ":")) + "\n",
                                   encoding="utf-8", newline="\n")
        publish_directory(staged, args.output)
    print(json.dumps({"programs": len(data["programs"]), "sha256": digest(args.output / name)}))
