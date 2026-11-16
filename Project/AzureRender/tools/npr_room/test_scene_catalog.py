from pathlib import Path
import tempfile
import unittest

from scene_catalog import build_static_exchange
from skinned_mesh_export import UnityScene


GUID = "1" * 32


def mesh_object(identifier, active, name, position):
    return f"""--- !u!1 &{identifier}
GameObject:
  m_Name: {name}
  m_IsActive: {active}
  m_Component:
  - component: {{fileID: {identifier + 1}}}
  - component: {{fileID: {identifier + 2}}}
  - component: {{fileID: {identifier + 3}}}
--- !u!4 &{identifier + 1}
Transform:
  m_GameObject: {{fileID: {identifier}}}
  m_LocalPosition: {{x: 0, y: 0, z: {position}}}
  m_LocalRotation: {{x: 0, y: 0, z: 0, w: 1}}
  m_LocalScale: {{x: 1, y: 1, z: 1}}
  m_Father: {{fileID: 0}}
--- !u!33 &{identifier + 2}
MeshFilter:
  m_GameObject: {{fileID: {identifier}}}
  m_Mesh: {{fileID: 4300000, guid: {GUID}, type: 2}}
--- !u!23 &{identifier + 3}
MeshRenderer:
  m_GameObject: {{fileID: {identifier}}}
  m_Enabled: 1
  m_StaticBatchInfo: {{firstSubMesh: 0, subMeshCount: 0}}
"""


class SceneCatalogTests(unittest.TestCase):
    def test_obeys_platform_activation_and_keeps_shared_mesh_resources(self):
        with tempfile.TemporaryDirectory() as tmp:
            p = Path(tmp) / "scene.unity"
            p.write_text(mesh_object(10, 0, "desktop", 2)
                         + mesh_object(20, 1, "mobile", 3)
                         + mesh_object(30, 1, "shared", 4), encoding="utf-8")
            source = UnityScene(p)
            exchange, stats = build_static_exchange(
                source, "scene", {GUID: {"kind": "static", "media": "meshes/test.glb"}}, {10: True, 20: False}
            )
        self.assertEqual(exchange["resources"], [{"id": GUID, "path": "meshes/test.glb"}])
        self.assertEqual([node["name"] for node in exchange["nodes"]], ["desktop", "shared"])
        self.assertEqual(exchange["nodes"][0]["localTransform"]["matrix"][14], -2)
        self.assertEqual(stats["renderables"], 2)
        self.assertEqual(stats["inactive"], 1)


if __name__ == "__main__":
    unittest.main()
