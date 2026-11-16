import ctypes
import json
import os
from pathlib import Path
import tempfile
import threading
import unittest
import wave

from audio_catalog import publish_directory, stage_catalog_audio


class AudioCatalogTests(unittest.TestCase):
    def test_preserves_catalog_aliases_with_one_payload_for_identical_sources(self):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            source = root / "project"
            (source / "Assets").mkdir(parents=True)
            first = source / "Assets/first.wav"
            with wave.open(str(first), "wb") as clip:
                clip.setparams((1, 2, 48000, 4, "NONE", "not compressed"))
                clip.writeframes(b"\0" * 8)
            second = source / "Assets/second.wav"
            second.write_bytes(first.read_bytes())
            kind = {"m_ClassName": "UnityEngine.AudioClip"}
            report = {
                "assets": {"Assets/first.wav": "1" * 32, "Assets/second.wav": "2" * 32},
                "sceneClosure": ["Assets/second.wav"],
                "catalog": {
                    "sha256": "3" * 64,
                    "locations": [
                        {"internalId": "ref/first", "resourceType": kind},
                        {"internalId": "ref/second", "resourceType": kind},
                    ],
                    "assetCandidates": {
                        "ref/first": {"paths": ["Assets/first.wav"]},
                        "ref/second": {"paths": ["Assets/second.wav"]},
                    },
                    "keys": [{"kind": 0, "value": "both", "locations": [0, 1]}],
                },
            }
            for clip, guid in zip((first, second), report["assets"].values(), strict=True):
                clip.with_suffix(".wav.meta").write_text(f"guid: {guid}\n", encoding="utf-8")
            output = root / "staged"
            stats = stage_catalog_audio(source, report, output)
            self.assertEqual(stats["uniquePayloads"], 1)
            self.assertEqual(stats["catalogLocations"], 2)
            files = list((output / "media").glob("*.wav"))
            self.assertEqual(len(files), 1)
            self.assertEqual(files[0].read_bytes(), first.read_bytes())
            mapping = json.loads((output / "audio-catalog.json").read_text(encoding="utf-8"))
            self.assertEqual(set(mapping["slots"]), {"1" * 32, "2" * 32})
            self.assertEqual(mapping["aliases"], [{"kind": 0, "value": "both", "slots": ["1" * 32, "2" * 32]}])
            with self.assertRaises(FileExistsError):
                stage_catalog_audio(source, report, output)


    @unittest.skipUnless(os.name == "nt", "Windows directory sharing")
    def test_publishes_after_a_transient_directory_handle_closes(self):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            staged = root / "staged"
            staged.mkdir()
            out = root / "catalog"
            kernel = ctypes.WinDLL("kernel32", use_last_error=True)
            kernel.CreateFileW.argtypes = [
                ctypes.c_wchar_p, ctypes.c_uint32, ctypes.c_uint32, ctypes.c_void_p,
                ctypes.c_uint32, ctypes.c_uint32, ctypes.c_void_p,
            ]
            kernel.CreateFileW.restype = ctypes.c_void_p
            kernel.CloseHandle.argtypes = [ctypes.c_void_p]
            handle = kernel.CreateFileW(str(staged), 1, 3, None, 3, 0x02000000, None)
            self.assertNotEqual(handle, ctypes.c_void_p(-1).value, ctypes.get_last_error())
            release = threading.Timer(0.25, kernel.CloseHandle, [handle])
            release.start()
            try:
                publish_directory(staged, out)
            finally:
                release.join()
            self.assertTrue(out.is_dir())


if __name__ == "__main__":
    unittest.main()
