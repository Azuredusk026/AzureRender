from pathlib import Path
import tempfile
import unittest

from reference_inventory import static_inventory


SCENE_ID = "1" * 32
PREFAB_ID = "2" * 32
CLIP_ID = "3" * 32
UNUSED_ID = "4" * 32
MISSING_ID = "5" * 32
BUILTIN_ID = "0000000000000000f000000000000000"


class ReferenceInventoryTests(unittest.TestCase):
    def test_walks_transitive_references_and_preserves_object_ids(self):
        with tempfile.TemporaryDirectory() as temp:
            root = Path(temp)

            def write(name, body):
                target = root / name
                target.parent.mkdir(parents=True, exist_ok=True)
                target.write_text(body, encoding="utf-8")

            def asset(name, guid, body):
                write(name, body)
                write(name + ".meta", f"fileFormatVersion: 2\nguid: {guid}\n")

            write("ProjectSettings/EditorBuildSettings.asset", f"""
  m_Scenes:
  - enabled: 1
    path: Assets/Boot.unity
    guid: {SCENE_ID}
  m_configObjects: {{}}
""")
            asset("Assets/Boot.unity", SCENE_ID, f"""
%YAML 1.1
  prefab: {{fileID: 15, guid: {PREFAB_ID}, type: 3}}
  shader: {{fileID: 7, guid: {BUILTIN_ID}, type: 0}}
  missing: {{fileID: 9, guid: {MISSING_ID}, type: 3}}
  cleared: {{fileID: 0, guid: {UNUSED_ID}, type: 3}}
""".lstrip())
            asset("Assets/Props.prefab", PREFAB_ID, f"""%YAML 1.1
  clip: {{fileID: 7400000,
         guid: {CLIP_ID}, type: 2}}
""")
            asset("Assets/Idle.anim", CLIP_ID, "%YAML 1.1\n")
            asset("Assets/Unused.asset", UNUSED_ID, "%YAML 1.1\n")
            asset("Assets/Resources/Menu.asset", "a" * 32, "%YAML 1.1\n")
            report = static_inventory(root)

            self.assertEqual(report["scenes"], [{"path": "Assets/Boot.unity", "guid": SCENE_ID}])
            self.assertEqual(
                set(report["sceneClosure"]), {"Assets/Boot.unity", "Assets/Props.prefab", "Assets/Idle.anim"}
            )
            self.assertEqual(
                report["references"]["Assets/Props.prefab"],
                [{"guid": CLIP_ID, "fileID": 7400000, "type": 2}],
            )
            self.assertEqual(report["unresolvedGuids"], {MISSING_ID: ["Assets/Boot.unity"]})
            self.assertEqual(report["resources"], {"Menu": ["Assets/Resources/Menu.asset"]})
            self.assertEqual(report, static_inventory(root))


if __name__ == "__main__":
    unittest.main()
