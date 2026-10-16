import importlib.util
import json
import tempfile
import unittest
from pathlib import Path

class SceneExchangeTests(unittest.TestCase):
 def load(self):
  path=Path(__file__).parent/'blender/export_scene_exchange.py'
  self.assertTrue(path.exists(),'Scene exporter is required')
  spec=importlib.util.spec_from_file_location('scene_export',path);module=importlib.util.module_from_spec(spec);spec.loader.exec_module(module);return module
 def test_shared_instance_and_atomic_invalid_export(self):
  module=self.load()
  with tempfile.TemporaryDirectory() as directory:
   root=Path(directory);(root/'panel.glb').write_bytes(b'fixture')
   data=dict(schemaVersion=1,id='room',unitMeters=1,upAxis='Y',resources=[dict(id='mesh',path='panel.glb')],nodes=[dict(id='root'),dict(id='a',parent='root',meshRef='mesh'),dict(id='b',parent='root',meshRef='mesh',localTransform=dict(scale=[-1,2,1]))])
   path=root/'room.azuresceneexchange';module.write_exchange(data,path)
   self.assertEqual(len(json.loads(path.read_text())['resources']),1)
   before=path.read_bytes();data['nodes'][0]['parent']='a'
   with self.assertRaisesRegex(ValueError,'cycle'):module.write_exchange(data,path)
   self.assertEqual(path.read_bytes(),before)
 def test_rejects_missing_dependencies_and_unknown_version(self):
  module=self.load()
  with tempfile.TemporaryDirectory() as directory:
   root=Path(directory);data=dict(schemaVersion=1,id='room',unitMeters=1,upAxis='Y',resources=[dict(id='mesh',path='missing.glb')],nodes=[])
   with self.assertRaisesRegex(ValueError,'resource'):module.write_exchange(data,root/'scene.azuresceneexchange')
   data['schemaVersion']=2
   with self.assertRaisesRegex(ValueError,'version'):module.write_exchange(data,root/'scene.azuresceneexchange')
if __name__=='__main__':unittest.main()
