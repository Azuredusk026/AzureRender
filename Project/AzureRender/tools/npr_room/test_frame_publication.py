import hashlib
import json
from pathlib import Path
import tempfile
import unittest
from unittest.mock import patch
import numpy as np
from PIL import Image

from tools.npr_room import frame_publication


class FramePublicationTests(unittest.TestCase):
    def setUp(self):
        self.temporary = tempfile.TemporaryDirectory()
        self.addCleanup(self.temporary.cleanup)
        self.root = Path(self.temporary.name)
        self.source = self.root / "capture"
        self.target = self.root / "assets"
        self.source.mkdir()
        self.target.mkdir()
        self.stem = "Night-00"
        self.names = [self.stem + ".png", self.stem + "-depth.f32", self.stem + ".json"]
        Image.new("RGB", (2, 2), (17, 31, 73)).save(self.source / self.names[0])
        (self.source / self.names[1]).write_bytes(np.array([0, .2, .6, 1], dtype="<f4").tobytes())
        record = {"weather": "Night", "index": 0, "frame": 44, "metadataOnly": False,
                  "width": 2, "height": 2, "depthGraphicsFormat": "R32_SFloat",
                  "color": self.names[0], "depth": self.names[1]}
        (self.source / self.names[2]).write_text(json.dumps(record), encoding="utf-8")
        digest = lambda name: hashlib.sha256((self.source / name).read_bytes()).hexdigest()
        self.ready = {"frame": 44, "colorSHA256": digest(self.names[0]),
                      "depthSHA256": digest(self.names[1]), "metadataSHA256": digest(self.names[2])}
        (self.source / (self.stem + ".ready")).write_text(json.dumps(self.ready), encoding="utf-8")

    def test_publishes_validated_payloads_and_descriptor_then_acknowledges(self):
        expected = {name: (self.source / name).read_bytes() for name in self.names}
        proof = frame_publication.publish_ready_frame(self.source, self.target, self.stem)
        self.assertEqual(proof["frame"], 44)
        self.assertEqual(proof["bytes"], sum(map(len, expected.values())))
        self.assertEqual({p.name: p.read_bytes() for p in self.target.iterdir()}, expected)
        self.assertEqual(list(self.source.iterdir()), [])

    def test_recovers_payload_move_and_committed_descriptor_retries(self):
        (self.source / self.names[0]).rename(self.target / self.names[0])
        frame_publication.publish_ready_frame(self.source, self.target, self.stem)
        (self.source / (self.stem + ".ready")).write_text(json.dumps(self.ready), encoding="utf-8")
        proof = frame_publication.publish_ready_frame(self.source, self.target, self.stem)
        self.assertEqual(proof["frame"], 44)
        self.assertEqual(list(self.source.iterdir()), [])

    def test_failed_move_rolls_back_and_corrupt_payload_stays_unpublished(self):
        from tools.npr_room.catalog_storage import publish_directory
        before = {p.name: p.read_bytes() for p in self.source.iterdir()}
        def move(source, target):
            if source.parent == self.source and source.suffix == ".f32":
                raise OSError("injected depth move failure")
            publish_directory(source, target)
        with patch.object(frame_publication, "publish_directory", side_effect=move):
            with self.assertRaisesRegex(OSError, "injected"):
                frame_publication.publish_ready_frame(self.source, self.target, self.stem)
        self.assertEqual({p.name: p.read_bytes() for p in self.source.iterdir()}, before)
        self.assertEqual(list(self.target.iterdir()), [])
        (self.source / self.names[1]).write_bytes(b"invalid")
        with self.assertRaises(ValueError):
            frame_publication.publish_ready_frame(self.source, self.target, self.stem)
        self.assertEqual(list(self.target.iterdir()), [])
