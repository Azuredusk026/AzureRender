import copy
import json
import math
from pathlib import Path
import tempfile
import unittest
from unittest.mock import patch

import numpy as np

from tools.npr_room import original_room_atmosphere as atmosphere
from tools.npr_room.reference_inventory import digest
from tools.npr_room.windows_room_fog import capture_original_view


class OriginalRoomAtmosphereTests(unittest.TestCase):
    def setUp(self):
        self.temporary = tempfile.TemporaryDirectory()
        self.addCleanup(self.temporary.cleanup)
        self.root = Path(self.temporary.name)
        self.bundle = self.root / "bundle"
        self.bundle.mkdir()
        self.assets = self.root / "assets"
        self.assets.mkdir()
        matrix = np.diag([1., 1., -1., 1.])
        matrix[:3, 3] = [-3, -4, 5]
        focal = 1 / math.tan(math.radians(35) / 2)
        projection = np.array([[focal * 9/16, 0, 0, 0], [0, focal, 0, 0],
                               [0, 0, -1.0001, -.100005], [0, 0, -1, 0]])
        self.record = {"width": 1280, "height": 720, "near": .05, "far": 1000, "reversedZ": True,
                       "position": {"x": 3, "y": 4, "z": 5}, "weather": "Night", "simulationTime": 2,
                       "worldToCamera": {f"m{r}{c}": matrix[r, c] for r in range(4) for c in range(4)},
                       "projection": {f"m{r}{c}": projection[r, c] for r in range(4) for c in range(4)}}
        self.frame = self.root / "frame.json"
        self.frame.write_text(json.dumps(self.record), encoding="utf-8")
        identity = digest(self.frame)
        (self.bundle / "environment.hdr").write_bytes(b"environment")
        (self.bundle / "fog.azfog").write_bytes(b"fog")
        self.recipe = {"base": {"drawables": {name: {"transform": index} for name, index in
                                                (("skyMesh", 1), ("cloudMesh", 2), ("fogMesh", 3))}},
                       "windowsReference": {"Night": {"sha256": "recipe"}}}
        self.composition = {"mode": "Night", "seconds": 2, "capturedFrameSHA256": identity,
                            "skySHA256": "sky", "cloudSHA256": "cloud", "environmentSHA256": digest(self.bundle / "environment.hdr")}
        self.sky = {"mode": "Night", "rawSHA256": "sky", "preparation": {
            "frameSHA256": identity, "sourceSeconds": 2, "referenceSHA256": "recipe"}}
        self.cloud = {"mode": "Night", "program": "cozy-cloud", "frames": [{"rawSHA256": "cloud", "inputs": {
            "capturedFrameSHA256": identity, "seconds": 2, "referenceSHA256": "recipe"}}]}
        self.fog = {"volume": {"volumeSHA256": digest(self.bundle / "fog.azfog")}, "source": {
            "captureCameraSHA256": identity, "camera": capture_original_view(self.record), "weather": {
                "capturedFrameSHA256": identity, "mode": "Night", "seconds": 2, "referenceSHA256": "recipe"}}}
        ids = [1, 2, 3, 7292, 7866]
        self.level = {"renderSettings": {"exposure": .5},
                      "sceneImports": {"RoomScene": {"nodeMap": {
                          ("unity-batch-" if i > 3 else "unity-") + str(i): "node-" + str(i) for i in ids}}},
                      "resources": [{"id": "city", "asset": "assets:/window-city.glb"}],
                      "nodes": [{"id": "node-" + str(i), "components": {}} for i in ids] + [
                          {"id": "city-node", "resourceId": "city", "components": {}}]}

    def install(self):
        for name, value in (("composition.json", self.composition), ("sky-native.json", self.sky),
                            ("cloud-native.json", self.cloud), ("fog-native.json", self.fog),
                            ("atmosphere-source-lock.json", self.recipe)):
            (self.bundle / name).write_text(json.dumps(value), encoding="utf-8")
        return atmosphere.install(self.frame, self.bundle, self.assets, self.level)

    def test_installs_relative_fog_and_launch_environment_with_source_immutability(self):
        original_frame = self.frame.read_bytes()
        result = self.install()
        self.assertEqual(result["environmentPath"], "assets/original-atmosphere/environment.hdr")
        self.assertEqual(self.level["viewFog"]["asset"], "assets:/original-atmosphere/fog.azfog")
        self.assertEqual(self.level["renderSettings"]["skyMode"], 1)
        self.assertEqual(self.level["renderSettings"]["exposure"], .5)
        self.assertTrue(all(not node["components"]["azure.renderable"]["data"]["visible"] for node in self.level["nodes"]))
        self.assertEqual(self.frame.read_bytes(), original_frame)
        self.assertEqual((self.assets / "original-atmosphere/environment.hdr").read_bytes(), b"environment")
        with self.assertRaises(FileExistsError):
            self.install()

    def test_rejects_source_camera_and_payload_mismatches_transactionally(self):
        for mode in ("frame", "camera", "payload", "clock", "recipe"):
            with self.subTest(mode=mode):
                fog, composition = copy.deepcopy(self.fog), copy.deepcopy(self.composition)
                level = copy.deepcopy(self.level)
                if mode == "frame":
                    self.fog["source"]["weather"]["capturedFrameSHA256"] = "b" * 64
                elif mode == "camera":
                    self.fog["source"]["camera"]["farPlane"] = 500
                elif mode == "payload":
                    self.composition["environmentSHA256"] = "b" * 64
                elif mode == "clock":
                    self.fog["source"]["weather"]["seconds"] = 3
                else:
                    self.fog["source"]["weather"]["referenceSHA256"] = "different"
                with self.assertRaises(ValueError):
                    self.install()
                self.assertEqual(self.level, level)
                self.assertEqual(list(self.assets.iterdir()), [])
                self.fog, self.composition = fog, composition

    def test_failed_publication_keeps_level_and_assets_unmodified(self):
        level = copy.deepcopy(self.level)
        with patch.object(atmosphere, "publish_directory", side_effect=OSError("publication failed")):
            with self.assertRaisesRegex(OSError, "publication failed"):
                self.install()
        self.assertEqual(self.level, level)
        self.assertEqual(list(self.assets.iterdir()), [])
