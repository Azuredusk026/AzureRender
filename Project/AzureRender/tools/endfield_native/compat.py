"""Adapt the CRC-verified protocol-3 catalog observed in game 1.0.14.

The upstream Endfield Extractor parser accepts the group body, but this build
adds a four-byte revision before it and a fourteen-byte footer before the CRC.
Keep the original input and metadata alongside every normalized result.
"""
import struct
import zlib


def normalize_legacy_catalog(decrypted: bytes) -> tuple[bytes, dict]:
    if len(decrypted) < 56:
        raise ValueError("Truncated catalog")
    protocol = struct.unpack_from("<I", decrypted)[0]
    if protocol != 3:
        raise ValueError(f"Unsupported catalog protocol: {protocol}")
    expected = struct.unpack_from("<I", decrypted, len(decrypted) - 4)[0]
    if zlib.crc32(decrypted[12:-4]) != expected:
        raise ValueError("Catalog CRC mismatch")
    revision, name_length = struct.unpack_from("<IH", decrypted, 12)
    if not 0 < name_length <= 128 or 18 + name_length + 25 > len(decrypted) - 18:
        raise ValueError("Unsupported catalog group header")
    name = decrypted[18:18 + name_length].decode("utf-8", errors="strict")
    if not name.isprintable():
        raise ValueError("Invalid catalog group name")
    footer_version, footer_value, footer_flags = struct.unpack_from("<IqH", decrypted, len(decrypted) - 18)
    if footer_version != 3 or footer_flags != 0:
        raise ValueError("Unsupported catalog footer")
    return decrypted[:12] + decrypted[16:-18], {
        "protocol": protocol, "revision": revision, "group_name": name,
        "footer_version": footer_version, "footer_value": footer_value,
        "footer_flags": footer_flags, "crc_verified": True,
    }


def install_legacy_adapter(vfs):
    """Install only in the caller's isolated process; upstream files stay intact."""
    original = vfs.parse_blc

    def parse(decrypted, **expected):
        normalized, metadata = normalize_legacy_catalog(decrypted)
        group = original(normalized)
        expected_hash = expected.get("expected_group_hash")
        if expected_hash is not None and group.group_hash.upper() != expected_hash.upper():
            raise ValueError("Catalog group identity mismatch")
        count = expected.get("expected_chunk_count")
        size = expected.get("expected_chunks_length")
        if count is not None and len(group.chunks) != count:
            raise ValueError("Catalog chunk count mismatch")
        if size is not None and group.chunks_length != size:
            raise ValueError("Catalog chunk size mismatch")
        return group

    vfs.parse_blc = parse
    return original
