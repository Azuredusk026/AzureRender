"""Stage WAV playback copies for Recovery exports with zero-size headers."""

import argparse
from hashlib import sha256
import json
from pathlib import Path
from struct import Struct, pack_into
import tempfile

from tools.npr_room.catalog_storage import publish_directory
from tools.npr_room.reference_inventory import digest


WAVE_HEADER = Struct("<4sI4s4sIHHIIHH4sI")


def repair_wave_header(data):
    if len(data) < WAVE_HEADER.size:
        raise ValueError("WAV is too short for a Recovery PCM header")
    (riff, riff_size, wave, fmt, fmt_size, encoding, channels, rate, byte_rate,
     alignment, bits, samples, sample_bytes) = WAVE_HEADER.unpack_from(data)
    if (riff, wave) != (b"RIFF", b"WAVE"):
        raise ValueError("Expected RIFF/WAVE source")
    if riff_size and (samples != b"data" or sample_bytes):
        return None
    if riff_size or sample_bytes:
        raise ValueError("Both Recovery WAV lengths must be zero")
    payload_size = len(data) - WAVE_HEADER.size
    if ((fmt, fmt_size, encoding, bits, samples) != (b"fmt ", 16, 1, 16, b"data")
            or not 1 <= channels <= 8 or not 8000 <= rate <= 192000
            or alignment != channels * 2 or byte_rate != rate * alignment
            or not payload_size or payload_size % alignment or len(data) - 8 > 0xffffffff):
        raise ValueError("Invalid Recovery PCM layout")
    repaired = bytearray(data)
    pack_into("<I", repaired, 4, len(data) - 8)
    pack_into("<I", repaired, 40, payload_size)
    return repaired


def stage_audio_repairs(manifest, output):
    manifest = manifest.resolve(strict=True)
    if output.exists():
        raise FileExistsError(output)
    source_manifest = manifest.read_bytes()
    manifest_sha = sha256(source_manifest).hexdigest()
    catalog = json.loads(source_manifest)
    wavs = {slot["media"]: slot["sha256"] for slot in catalog["slots"].values()
            if Path(slot["media"]).suffix.lower() == ".wav"}
    repairs = {}
    with tempfile.TemporaryDirectory(prefix="recovery-wav-", dir=output.parent) as directory:
        staged = Path(directory) / "publish"
        staged.mkdir()
        for name, source_sha in sorted(wavs.items()):
            relative = Path(name)
            if relative.parent != Path("media") or relative.stem != source_sha:
                raise ValueError(f"Invalid WAV catalog path: {name}")
            source = (manifest.parent / relative).resolve(strict=True)
            if source.parent != manifest.parent / "media":
                raise ValueError(f"WAV resolves outside the catalog: {name}")
            data = source.read_bytes()
            if sha256(data).hexdigest() != source_sha:
                raise ValueError(f"WAV content changed: {name}")
            repaired = repair_wave_header(data)
            if repaired is None:
                continue
            repaired_sha = sha256(repaired).hexdigest()
            target = staged / f"{repaired_sha}.wav"
            target.write_bytes(repaired)
            if digest(target) != repaired_sha or digest(source) != source_sha:
                raise ValueError(f"WAV changed while writing: {name}")
            repairs[name] = {
                "sourceSHA256": source_sha, "sha256": repaired_sha, "media": target.name,
                "pcmSHA256": sha256(data[WAVE_HEADER.size:]).hexdigest(),
            }
        if digest(manifest) != manifest_sha:
            raise ValueError("Audio catalog changed while preparing WAV playback copies")
        report = {"formatVersion": 1, "sourceManifestSHA256": manifest_sha, "repairs": repairs}
        (staged / "audio-repairs.json").write_text(
            json.dumps(report, sort_keys=True, separators=(",", ":")) + "\n",
            encoding="utf-8", newline="\n",
        )
        publish_directory(staged, output)
    return report


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--catalog", type=Path, required=True)
    parser.add_argument("--output", type=Path, required=True)
    args = parser.parse_args()
    report = stage_audio_repairs(args.catalog, args.output)
    print(json.dumps({"wavRepairs": len(report["repairs"])}))


if __name__ == "__main__":
    main()
