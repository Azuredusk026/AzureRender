"""Verify immutable glTF overlay composition and CLI provenance."""
import copy,json,struct,subprocess,sys,tempfile,unittest
from pathlib import Path
try:
 from append_material_overlays import append_overlays
except ImportError:
 append_overlays=None
from convert_material_semantics import read_asset,write_asset

def model():
 return dict(asset=dict(version='2.0'),materials=[dict(name='cloth',extras=dict(azureSlotId='cloth'))],meshes=[dict(primitives=[dict(attributes=dict(POSITION=0,NORMAL=1,TEXCOORD_0=2,TEXCOORD_1=3,JOINTS_0=4,WEIGHTS_0=5),indices=6,material=0,targets=[dict(POSITION=7)])])],nodes=[dict(mesh=0,skin=0)],skins=[dict(joints=[1],inverseBindMatrices=8)],animations=[dict(name='idle',channels=[],samplers=[])],images=[],textures=[],samplers=[],buffers=[dict(byteLength=4)])

def config():
 surface=dict(schemaVersion=2,lighting='unlit',layers=[dict(role='base',texture=0,sampleMode='rgba',uv=dict(set=0,scaleOffset=[1,1,0,0],rotation=[1,0,0,1],speed=[0,0]))],tint=[1,1,1,1],blendTint=[0,0,0,0],clock=dict(rate=0,offsetSeconds=0),softIntersection=dict(distance=1,bias=0,enabled=False),highlightExpansion=dict(threshold=1,gain=0),postExposureWeight=0,globalColorWeight=0,depthCompare='less-or-equal')
 return dict(schemaVersion=1,images=[dict(uri='data:image/png;base64,iVBORw0KGgo=')],samplers=[dict(wrapS=10497,wrapT=10497,minFilter=9729,magFilter=9729)],textures=[dict(source=0,sampler=0)],overlays=[dict(sourceSlot='cloth',uvSet=1,material=dict(name='glow',alphaMode='BLEND',doubleSided=True,extras=dict(azureRenderMaterial=dict(schemaVersion=2,**{'class':'generic'},features=['unlit-surface','unlit-texture-layers'],surface=surface))))])

class OverlayTests(unittest.TestCase):
 def setUp(self):self.assertIsNotNone(append_overlays,'Explicit overlay composition is missing')
 def test_skin_morph_animation_and_original_graph_are_preserved(self):
  d=model();c=config();original=copy.deepcopy(d);cfg=copy.deepcopy(c);out,rows=append_overlays(d,c)
  self.assertEqual(d,original);self.assertEqual(c,cfg)
  for key in ['nodes','skins','animations','buffers']:self.assertEqual(out[key],d[key])
  base=d['meshes'][0]['primitives'][0];overlay=out['meshes'][0]['primitives'][1]
  self.assertEqual(out['meshes'][0]['primitives'][0],base);self.assertEqual(overlay['targets'],base['targets']);self.assertEqual(overlay['indices'],base['indices'])
  self.assertEqual(overlay['attributes']['TEXCOORD_0'],base['attributes']['TEXCOORD_1'])
  for key in ['POSITION','NORMAL','JOINTS_0','WEIGHTS_0']:self.assertEqual(overlay['attributes'][key],base['attributes'][key])
  self.assertEqual(overlay['material'],1);self.assertEqual(rows[0]['primitives'],1)
 def test_local_texture_tables_are_remapped_once_for_multiple_layers(self):
  d=model();d['images']=[dict(uri='original.png')];d['textures']=[dict(source=0)];d['samplers']=[dict()];c=config();s=c['overlays'][0]['material']['extras']['azureRenderMaterial']['surface'];s['layers'].append(copy.deepcopy(s['layers'][0]));s['layers'][1]['role']='blend';s['coverageLayer']=dict(texture=0);s['baseUvDistortion']=dict(texture=0);c['overlays'][0]['material']['pbrMetallicRoughness']=dict(baseColorTexture=dict(index=0))
  other=copy.deepcopy(c['overlays'][0]);other['material']['name']='annotation';c['overlays'].append(other);out,rows=append_overlays(d,c)
  self.assertEqual(out['textures'][1],dict(source=1,sampler=1));self.assertEqual(len(out['meshes'][0]['primitives']),3)
  for m in out['materials'][1:]:
   s=m['extras']['azureRenderMaterial']['surface'];self.assertEqual([x['texture'] for x in s['layers']],[1,1]);self.assertEqual(s['coverageLayer']['texture'],1);self.assertEqual(s['baseUvDistortion']['texture'],1);self.assertEqual(m['pbrMetallicRoughness']['baseColorTexture']['index'],1)
 def test_missing_slot_uv_and_bad_indices_fail_without_mutation(self):
  for case in ['slot','uv','texture','image','sampler','boolean','duplicate','unknown']:
   d=model();c=config();original=copy.deepcopy(d)
   if case=='slot':c['overlays'][0]['sourceSlot']='missing'
   if case=='uv':c['overlays'][0]['uvSet']=2
   if case=='texture':c['overlays'][0]['material']['extras']['azureRenderMaterial']['surface']['layers'][0]['texture']=2
   if case=='image':c['textures'][0]['source']=3
   if case=='sampler':c['textures'][0]['sampler']=True
   if case=='boolean':c['overlays'][0]['uvSet']=True
   if case=='duplicate':c['overlays'].append(copy.deepcopy(c['overlays'][0]))
   if case=='unknown':c['silentFallback']=True
   with self.subTest(case=case),self.assertRaises(ValueError):append_overlays(d,c)
   self.assertEqual(d,original)
 def test_rigid_annotation_uses_declared_uv_zero(self):
  d=model();d['nodes'][0].pop('skin');d.pop('skins');d.pop('animations');c=config();c['overlays'][0]['uvSet']=0;c['overlays'][0]['material']['name']='annotation';out,_=append_overlays(d,c);self.assertEqual(out['meshes'][0]['primitives'][1]['attributes'],d['meshes'][0]['primitives'][0]['attributes'])
 def test_cli_keeps_binary_chunk_and_rejects_existing_provenance(self):
  with tempfile.TemporaryDirectory() as tmp:
   p=Path(tmp);source=p/'source.glb';output=p/'overlay.glb';cfg=p/'config.json';tail=struct.pack('<II',4,0x004e4942)+b'1234';write_asset(source,model(),tail);source_bytes=source.read_bytes();cfg.write_text(json.dumps(config()));command=[sys.executable,str(Path(__file__).with_name('append_material_overlays.py')),'--input',str(source),'--config',str(cfg),'--output',str(output)]
   first=subprocess.run(command,capture_output=True,text=True);self.assertEqual(first.returncode,0,first.stderr);self.assertEqual(source.read_bytes(),source_bytes);self.assertEqual(read_asset(output)[1],tail);receipt=Path(str(output)+'.overlays.json');self.assertTrue(receipt.exists());saved=receipt.read_bytes();output.unlink();second=subprocess.run(command,capture_output=True,text=True);self.assertNotEqual(second.returncode,0);self.assertFalse(output.exists());self.assertEqual(receipt.read_bytes(),saved)

if __name__=='__main__':unittest.main()
