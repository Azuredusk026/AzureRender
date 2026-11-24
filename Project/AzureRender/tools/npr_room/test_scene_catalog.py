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
    def test_exports_selected_active_camera_with_parent_and_source_planes(self):
        camera = """--- !u!1 &50
GameObject:
  m_Name: MainCamera
  m_IsActive: 1
  m_Component:
  - component: {fileID: 51}
  - component: {fileID: 52}
--- !u!4 &51
Transform:
  m_GameObject: {fileID: 50}
  m_LocalPosition: {x: 2, y: 3, z: 4}
  m_LocalRotation: {x: 0, y: 0, z: 0, w: 1}
  m_LocalScale: {x: 1, y: 1, z: 1}
  m_Father: {fileID: 11}
--- !u!20 &52
Camera:
  m_GameObject: {fileID: 50}
  m_Enabled: 1
  m_projectionMatrixMode: 1
  m_FOVAxisMode: 0
  m_LensShift: {x: 0, y: 0}
  m_NormalizedViewPortRect: {x: 0, y: 0, width: 1, height: 1}
  near clip plane: 0.05
  far clip plane: 1500
  field of view: 35
  orthographic: 0
  orthographic size: 5
"""
        with tempfile.TemporaryDirectory() as tmp:
            path = Path(tmp) / "scene.unity"
            path.write_text(mesh_object(10, 1, "parent", 0) + camera, encoding="utf-8")
            scene = UnityScene(path)
            exchange, stats = build_static_exchange(scene, "scene", {}, {}, camera_id=52)
            with self.assertRaisesRegex(ValueError, "inactive"):
                build_static_exchange(scene, "scene", {}, {10: False}, camera_id=52)
        node = next(node for node in exchange["nodes"] if node["id"] == "unity-51")
        self.assertEqual(node["parent"], "unity-11")
        self.assertEqual(node["localTransform"]["matrix"][12:15], [2, 3, -4])
        self.assertEqual(node["camera"], {"enabled": True, "priority": 0, "orthographic": False,
                         "fieldOfView": 35, "orthographicHeight": 10, "nearPlane": 0.05, "farPlane": 1500})
        self.assertEqual(stats["cameras"], 1)

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
