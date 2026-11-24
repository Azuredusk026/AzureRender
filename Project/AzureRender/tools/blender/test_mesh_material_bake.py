"""Check independent UV inputs and mesh-target baking through Blender/glTF."""
import argparse
import json
from pathlib import Path
import struct
import sys

import bpy
import numpy as np

sys.path.insert(0, str(Path(__file__).resolve().parent))
from bake_material_channels import bake_material

parser = argparse.ArgumentParser(__doc__)
parser.add_argument('--output', type=Path, required=True)
args = parser.parse_args(sys.argv[sys.argv.index('--')+1:])
args.output.mkdir(parents=True, exist_ok=False)
bpy.ops.object.select_all(action='SELECT')
bpy.ops.object.delete(use_global=False)
mesh = bpy.data.meshes.new('Independent UV fixture')
mesh.from_pydata([(-1,-1,0),(1,-1,0),(1,1,0),(-1,1,0)], [], [(0,1,2),(0,2,3)])
uv0, uv1 = mesh.uv_layers.new(name='UV0'), mesh.uv_layers.new(name='UV1')
for loop in mesh.loops:
    x,y,_ = mesh.vertices[loop.vertex_index].co
    u,v = (x+1)/2,(y+1)/2
    uv0.data[loop.index].uv = (1-u,v)
    uv1.data[loop.index].uv = (u,v)
source = bpy.data.materials.new('Two independent UV maps')
source.use_nodes = True
mesh.materials.append(source)
nodes, links = source.node_tree.nodes, source.node_tree.links
bsdf = nodes.get('Principled BSDF')
bsdf.inputs['Roughness'].default_value = .25
bsdf.inputs['Metallic'].default_value = .75
coordinates = []
for name in ['UV0','UV1']:
    uv = nodes.new('ShaderNodeUVMap')
    uv.uv_map = name
    separate = nodes.new('ShaderNodeSeparateXYZ')
    links.new(uv.outputs[0], separate.inputs[0])
    coordinates.append(separate)
combine = nodes.new('ShaderNodeCombineXYZ')
for lane, (socket, low, scale) in enumerate([(coordinates[0].outputs['X'],.2,.6),
        (coordinates[1].outputs['Y'],.1,.5),(coordinates[1].outputs['X'],.3,.4)]):
    compare = nodes.new('ShaderNodeMath'); compare.operation='GREATER_THAN'
    compare.inputs[1].default_value=.5; links.new(socket,compare.inputs[0])
    value = nodes.new('ShaderNodeMath'); value.operation='MULTIPLY_ADD'
    value.inputs[1].default_value=scale; value.inputs[2].default_value=low
    links.new(compare.outputs[0],value.inputs[0]); links.new(value.outputs[0],combine.inputs[lane])
links.new(combine.outputs[0],bsdf.inputs['Base Color'])
baked, receipt = bake_material(source,64,mesh=mesh,uv_layer='UV1')
image = bpy.data.images[receipt['images']['base']]
pixels = np.array(image.pixels[:],dtype=np.float64).reshape(64,64,4)
decoded = np.where(pixels[:,:,:3]<=.04045,pixels[:,:,:3]/12.92,((pixels[:,:,:3]+.055)/1.055)**2.4)
maximum = 0.
for x,y in [(16,16),(48,16),(16,48),(48,48)]:
    expected = [.8 if x<32 else .2,.6 if y>32 else .1,.7 if x>32 else .3]
    maximum=max(maximum,float(np.max(abs(decoded[y,x]-expected))))
assert maximum<.004, maximum
data = np.array(bpy.data.images[receipt['images']['orm']].pixels[:]).reshape(64,64,4)
assert np.max(abs(data[16:48,16:48,1:3]-[.25,.75]))<.004
for kwargs in ({}, dict(mesh=mesh,uv_layer='missing')):
    try:
        bake_material(source,32,**kwargs)
    except ValueError:
        pass
    else:
        raise AssertionError('Independent UV maps require a valid mesh target')
assert mesh.name in bpy.data.meshes and source.name in bpy.data.materials
obj = bpy.data.objects.new('Baked fixture',mesh)
bpy.context.collection.objects.link(obj)
mesh.materials.clear(); mesh.materials.append(baked)
mesh.uv_layers.active=mesh.uv_layers['UV1']
mesh.uv_layers['UV1'].active_render=True
mesh.calc_tangents(uvmap='UV1')
expected_signs=np.array([loop.bitangent_sign for loop in mesh.loops])
assert np.all(expected_signs>.99)
bpy.context.view_layer.objects.active=obj; obj.select_set(True)
target=args.output/'fixture.glb'
bpy.ops.export_scene.gltf(filepath=str(target),export_format='GLB',use_selection=True,export_tangents=True)
binary=target.read_bytes();size=struct.unpack_from('<I',binary,12)[0];gltf=json.loads(binary[20:20+size])
material=gltf['materials'][0]
assert material['pbrMetallicRoughness']['baseColorTexture']['texCoord']==1
assert material['pbrMetallicRoughness']['metallicRoughnessTexture']['texCoord']==1
assert material['normalTexture']['texCoord']==1
assert 'TEXCOORD_1' in gltf['meshes'][0]['primitives'][0]['attributes']
assert 'TANGENT' in gltf['meshes'][0]['primitives'][0]['attributes']
tangent=gltf['accessors'][gltf['meshes'][0]['primitives'][0]['attributes']['TANGENT']]
view=gltf['bufferViews'][tangent['bufferView']]
blob=binary[28+size:]
values=np.ndarray((tangent['count'],4),dtype='<f4',buffer=blob,
    offset=view.get('byteOffset',0)+tangent.get('byteOffset',0),strides=(view.get('byteStride',16),4))
assert np.all(values[:,0]>.99) and np.all(values[:,3]>.99),'Exported tangents must preserve the selected authoring UV basis'
periodic=bpy.data.materials.new('Periodic UV control');periodic.use_nodes=True
periodic.node_tree.nodes.get('Principled BSDF').inputs['Base Color'].default_value=(.2,.4,.6,1)
control,control_receipt=bake_material(periodic,32)
control_pixels=np.array(bpy.data.images[control_receipt['images']['base']].pixels[:]).reshape(32,32,4)[:,:,:3]
control_linear=np.where(control_pixels<=.04045,control_pixels/12.92,((control_pixels+.055)/1.055)**2.4)
assert np.max(abs(control_linear-[.2,.4,.6]))<.004
padded=mesh.copy()
for item in padded.uv_layers['UV1'].data:item.uv=np.asarray(item.uv)*.5+.25
padding_material,padding_receipt=bake_material(source,64,mesh=padded,uv_layer='UV1')
padding_pixels=np.array(bpy.data.images[padding_receipt['images']['base']].pixels[:]).reshape(64,64,4)
assert padding_pixels[32,12,3]>.99,'Mesh atlas padding must extend the baked opaque surface'
report=dict(status='passed',maximumLinearColorError=maximum,uvTarget='UV1',texCoord=1,
            sourceMeshPreserved=True,sourceMaterialPreserved=True,periodicControlPassed=True,
            atlasPaddingPassed=True,receipt=receipt)
(args.output/'summary.json').write_text(json.dumps(report,indent=2),encoding='utf-8')
print(json.dumps(report),flush=True)
