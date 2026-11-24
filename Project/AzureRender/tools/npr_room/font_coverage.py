"""Check story code points against the source font routing and staged outlines."""

import argparse
from collections import Counter
import ctypes as ct
from hashlib import sha256
import json
import os
from pathlib import Path
from unicodedata import category

from tools.npr_room.reference_inventory import digest, group_paths_by_guid
from tools.npr_room.skinned_mesh_export import UnityScene


SUPPLIER_FIELDS = {
    "En": "English", "Ja": "Japanese", "Ko": "Korean", "Pt": "English", "Ru": "Russian",
    "ZhHans": "ChineseSimplified", "ZhHant": "ChineseTraditional",
}


def required_codepoints(texts):
    return {ord(char) for text in texts for char in text if category(char) not in ("Cc", "Cf")}


def glyph_rect_inside(glyph, width, height, atlas_count):
    rect = glyph["m_GlyphRect"]
    x, y, w, h = (rect[f"m_{field}"] for field in ("X", "Y", "Width", "Height"))
    return (0 <= glyph["m_AtlasIndex"] < atlas_count and min(x, y, w, h) >= 0
            and x + w <= width and y + h <= height)


def fallback_coverage(root, font):
    seen, visited, covered = set(), [], {}

    def visit(pointer, local_guid):
        if not pointer["fileID"]:
            return
        key = (pointer.get("guid", local_guid), pointer["fileID"])
        if key in seen:
            return
        seen.add(key)
        visited.append(key)
        codepoints, fallbacks = font(key)
        for codepoint in codepoints:
            covered.setdefault(codepoint, key)
        for child in fallbacks:
            visit(child, key[0])

    visit(root, None)
    return covered, visited


def outline_coverage(library_path, image_manifest):
    library_path = library_path.resolve(strict=True)
    if os.name == "nt":
        with os.add_dll_directory(str(library_path.parent)):
            freetype = ct.CDLL(str(library_path))
    else:
        freetype = ct.CDLL(str(library_path))
    signatures = {
        "FT_Init_FreeType": (ct.c_int, ct.POINTER(ct.c_void_p)),
        "FT_Done_FreeType": (ct.c_int, ct.c_void_p),
        "FT_New_Memory_Face": (ct.c_int, ct.c_void_p, ct.c_void_p, ct.c_long, ct.c_long, ct.POINTER(ct.c_void_p)),
        "FT_Done_Face": (ct.c_int, ct.c_void_p),
        "FT_Select_Charmap": (ct.c_int, ct.c_void_p, ct.c_uint),
        "FT_Get_First_Char": (ct.c_ulong, ct.c_void_p, ct.POINTER(ct.c_uint)),
        "FT_Get_Next_Char": (ct.c_ulong, ct.c_void_p, ct.c_ulong, ct.POINTER(ct.c_uint)),
    }
    for symbol, (result, *args) in signatures.items():
        function = getattr(freetype, symbol)
        function.restype, function.argtypes = result, args

    slots = json.loads(image_manifest.read_text(encoding="utf-8"))["slots"]
    root = image_manifest.resolve(strict=True).parent
    engine = ct.c_void_p()
    if freetype.FT_Init_FreeType(ct.byref(engine)):
        raise RuntimeError("Could not initialize FreeType")
    outlines = {}
    try:
        for guid, slot in slots.items():
            relative = Path(slot["media"])
            if relative.suffix not in (".otf", ".ttf"):
                continue
            file = (root / relative).resolve(strict=True)
            if file.parent != root / "media":
                raise ValueError("Font media resolves outside the image catalog")
            payload = file.read_bytes()
            if sha256(payload).hexdigest() != slot["sha256"]:
                raise ValueError(f"Font payload changed: {relative}")
            buffer = ct.create_string_buffer(payload)
            face = ct.c_void_p()
            if freetype.FT_New_Memory_Face(engine, buffer, len(payload), 0, ct.byref(face)):
                raise ValueError(f"Could not load font: {slot['source']}")
            try:
                if freetype.FT_Select_Charmap(face, int.from_bytes(b"unic", "big")):
                    raise ValueError(f"Font has no Unicode character map: {slot['source']}")
                codepoints = set()
                glyph = ct.c_uint()
                codepoint = freetype.FT_Get_First_Char(face, ct.byref(glyph))
                while glyph.value:
                    codepoints.add(codepoint)
                    codepoint = freetype.FT_Get_Next_Char(face, codepoint, ct.byref(glyph))
                outlines[guid] = codepoints
            finally:
                freetype.FT_Done_Face(face)
    finally:
        freetype.FT_Done_FreeType(engine)
    return outlines


def build_coverage(source, supplier, inventory, localization, outlines):
    source = source.resolve(strict=True)
    paths = group_paths_by_guid(inventory["assets"])
    prefab = UnityScene(source / supplier)
    records = [prefab.read(key) for key, (kind, _, _) in prefab.documents.items() if kind == 114]
    binding, = [record for record in records if "fontAssetJapanese" in record]
    fonts, cache = {}, {}

    def get_font(key):
        if key in cache:
            return cache[key]
        guid, file_id = key
        name, = paths[guid]
        asset = (source / name).resolve(strict=True)
        if not asset.is_relative_to(source / "Assets"):
            raise ValueError(f"Font resolves outside Assets: {name}")
        document = UnityScene(asset)
        data = document.read(file_id)
        atlas = data["m_AtlasTextures"]
        width, height = data["m_AtlasWidth"], data["m_AtlasHeight"]
        glyphs = {item["m_Index"] for item in data["m_GlyphTable"]
                  if glyph_rect_inside(item, width, height, len(atlas))}
        character_table = data["m_CharacterTable"]
        points = {item["m_Unicode"] for item in character_table if item["m_GlyphIndex"] in glyphs}
        outline_guid = data["m_SourceFontFile"].get("guid")
        if data["m_AtlasPopulationMode"] == 1 and outline_guid:
            points.update(outlines[outline_guid])
        cache[key] = points, data["m_FallbackFontAssetTable"]
        fonts[f"{guid}:{file_id}"] = {
            "source": name, "sha256": digest(asset), "atlasMode": data["m_AtlasPopulationMode"],
            "atlasTextures": atlas, "atlasDimensions": [width, height], "characterEntries": len(character_table),
            "glyphsOutsideAtlas": len(data["m_GlyphTable"]) - len(glyphs),
            "missingGlyphIndices": sum(item["m_GlyphIndex"] not in glyphs for item in character_table),
            "outlineGuid": outline_guid, "codepoints": len(points),
        }
        return cache[key]

    rows = {}
    for locale, field in SUPPLIER_FIELDS.items():
        root = binding[f"fontAsset{field}"]
        text = [row[locale] for row in localization]
        needed = required_codepoints(text)
        covered, chain = fallback_coverage(root, get_font)
        matches = Counter(covered[codepoint] for codepoint in needed if codepoint in covered)
        rows[locale] = {
            "texts": sum(bool(value) for value in text), "distinctCodepoints": len(needed),
            "mainFont": fonts[f"{chain[0][0]}:{chain[0][1]}"]["source"],
            "primaryCovered": matches[chain[0]], "fallbackCovered": sum(matches.values()) - matches[chain[0]],
            "missing": [f"U+{codepoint:04X}" for codepoint in sorted(needed - covered.keys())],
            "visitedFonts": [f"{guid}:{file_id}" for guid, file_id in chain],
        }
    return {
        "formatVersion": 1, "supplier": supplier, "supplierSHA256": digest(source / supplier),
        "localizationRows": len(localization), "fonts": fonts, "locales": rows,
        "sourceFontCodepoints": {guid: len(points) for guid, points in outlines.items()},
    }


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--source", type=Path, required=True)
    parser.add_argument("--inventory", type=Path, required=True)
    parser.add_argument("--story", type=Path, required=True)
    parser.add_argument("--images", type=Path, required=True)
    parser.add_argument("--freetype", type=Path, required=True)
    parser.add_argument("--output", type=Path, required=True)
    args = parser.parse_args()
    inventory = json.loads(args.inventory.read_text(encoding="utf-8"))
    story = json.loads(args.story.read_text(encoding="utf-8"))
    outlines = outline_coverage(args.freetype, args.images)
    data = build_coverage(args.source, "Assets/GameObject/ProjectLifetimeScope.prefab",
                          inventory, story["localization"], outlines)
    data["inventorySHA256"] = digest(args.inventory)
    data["storySHA256"] = digest(args.story)
    data["imageManifestSHA256"] = digest(args.images)
    data["freeTypeSHA256"] = digest(args.freetype)
    with args.output.open("x", encoding="utf-8", newline="\n") as stream:
        json.dump(data, stream, ensure_ascii=False, sort_keys=True, separators=(",", ":"))
        stream.write("\n")
    print(json.dumps({key: len(row["missing"]) for key, row in data["locales"].items()}))


if __name__ == "__main__":
    main()
