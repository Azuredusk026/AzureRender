"""Check package transactions, portable mounts, provenance and immutable hashes."""
import json
import pathlib
import tempfile
import unittest
import build_game


class GamePackageRules(unittest.TestCase):
    def setUp(self):
        self.temp = tempfile.TemporaryDirectory(prefix="azure_pack_rules_")
        self.root = pathlib.Path(self.temp.name)
        self.project = self.root / "source/project.azureproject"
        self.project.parent.mkdir()
        (self.project.parent / "content").mkdir()
        self.project.write_text(json.dumps({"schemaVersion": 1, "id": "game", "name": "Game",
            "mounts": [{"name": "assets", "path": "content"}], "startupScene": "assets:/start.azurelevel"}))
        (self.project.parent / "content/start.azurelevel").write_text('{"schemaVersion":1}')
        self.install = self.root / "engine"
        share = self.install / "share/AzureRender"
        (self.install / "bin").mkdir(parents=True)
        for name in ["AzurePlayer.exe", "AzureRender.exe", "AzureMetaGen.exe", "rmlui.dll"]:
            (self.install / "bin" / name).write_bytes(b"MZ fixture")
        for name in ["shaders/game-ui.vert.spv", "shaders/game-ui.frag.spv", "assets_public/fonts/LatoLatin-Regular.ttf"]:
            path = share / name
            path.parent.mkdir(parents=True, exist_ok=True)
            path.write_bytes(b"fixture")
        (share / "runtime-build.json").write_text(json.dumps({"configuration": "Release", "architecture": "x64", "engineVersion": "0.1"}))
        (share / "licenses").mkdir()
        for name in build_game.REQUIRED_LICENSES:
            (share / "licenses" / name).write_text("Upstream license fixture")
        self.output = self.root / "published"

    def tearDown(self):
        self.temp.cleanup()

    def validate(self, player, project):
        config = json.loads(project.read_text())
        root = project.parent / config["mounts"][0]["path"]
        if not (root / "start.azurelevel").is_file():
            raise ValueError("Startup missing")

    def build(self, **kwargs):
        return build_game.build_game(self.project, self.install, self.output, validator=self.validate, **kwargs)

    def test_portable_tree_and_hashes(self):
        cache = self.project.parent / "content/.azure"
        cache.mkdir()
        (cache / "private-machine-cache").write_text("cache")
        self.build()
        build_game.verify_package(self.output)
        self.assertEqual(json.loads((self.output / "game/project.azureproject").read_text())["mounts"][0]["path"], "assets")
        self.assertFalse((self.output / "bin/AzureRender.exe").exists())
        self.assertFalse((self.output / "bin/AzureMetaGen.exe").exists())
        self.assertFalse((self.output / "game/assets/.azure").exists())
        (self.output / "game/assets/start.azurelevel").write_text("tamper")
        with self.assertRaises(ValueError):
            build_game.verify_package(self.output)

    def test_failure_retains_existing_package(self):
        self.build()
        original = (self.output / "game_manifest.json").read_bytes()
        def fail(player, project):
            if project.parent.name == "game":
                raise ValueError("Candidate validation failed")
        with self.assertRaises(ValueError):
            build_game.build_game(self.project, self.install, self.output, replace=True, validator=fail)
        self.assertEqual((self.output / "game_manifest.json").read_bytes(), original)
        build_game.verify_package(self.output)
        self.assertFalse(list(self.root.glob(".published-candidate-*")))

    def test_successful_replace_and_conflict(self):
        self.build()
        with self.assertRaises(ValueError):
            self.build()
        (self.project.parent / "content/extra.txt").write_text("extra")
        self.build(replace=True)
        self.assertTrue((self.output / "game/assets/extra.txt").is_file())
        build_game.verify_package(self.output)

    def test_release_fonts_and_private_boundary(self):
        metadata = self.install / "share/AzureRender/runtime-build.json"
        metadata.write_text('{"configuration":"Debug","architecture":"x64"}')
        with self.assertRaises(ValueError):
            self.build()
        metadata.write_text('{"configuration":"Release","architecture":"x64"}')
        secret = self.project.parent / "content/assets_private"
        secret.mkdir()
        (secret / "secret.gltf").write_text("secret")
        with self.assertRaises(ValueError):
            self.build()
        secret.rename(secret.with_name("ordinary"))
        (self.install / "share/AzureRender/assets_public/fonts/LatoLatin-Regular.ttf").unlink()
        with self.assertRaises(ValueError):
            self.build()

    def test_output_cannot_overlap_inputs(self):
        with self.assertRaises(ValueError):
            build_game.build_game(self.project, self.install, self.project.parent / "out", validator=self.validate)
        self.output.mkdir()
        (self.output / "user-file.txt").write_text("preserve")
        with self.assertRaises(ValueError):
            self.build(replace=True)
        self.assertEqual((self.output / "user-file.txt").read_text(), "preserve")

    def test_private_mount_name_is_rejected(self):
        config = json.loads(self.project.read_text())
        config["mounts"][0]["name"] = "assets_private"
        self.project.write_text(json.dumps(config))
        with self.assertRaises(ValueError):
            self.build()

    def test_delivery_documents_are_immutable(self):
        (self.project.parent / "GAME-GUIDE.md").write_text("Controls and quest")
        (self.project.parent / "QUALITY.json").write_text('{"width":1920,"height":1080}')
        self.build()
        self.assertEqual((self.output / "GAME-GUIDE.md").read_text(), "Controls and quest")
        self.assertEqual(json.loads((self.output / "QUALITY.json").read_text())["width"], 1920)
        self.assertIn("--width 1920 --height 1080", (self.output / "start-game.cmd").read_text())
        (self.output / "GAME-GUIDE.md").write_text("tamper")
        with self.assertRaises(ValueError):
            build_game.verify_package(self.output)

    def test_delivery_document_directory_is_rejected(self):
        (self.project.parent / "GAME-GUIDE.md").mkdir()
        with self.assertRaises(ValueError):
            self.build()

    def test_delivery_resolution_rejects_command_text(self):
        (self.project.parent / "QUALITY.json").write_text('{"width":"1920 & echo injected","height":1080}')
        with self.assertRaises(ValueError):
            self.build()

    def test_delivery_quality_requires_an_object(self):
        (self.project.parent / "QUALITY.json").write_text('[]')
        with self.assertRaises(ValueError):
            self.build()


if __name__ == "__main__":
    unittest.main()
