"""Run with Blender --background --factory-startup --python this_file."""
import sys
import json
from pathlib import Path
sys.path.insert(0, str(Path(__file__).resolve().parent))
import bpy
from bake_material_channels import bake_material

bpy.ops.mesh.primitive_plane_add()
obj = bpy.context.object
mat = bpy.data.materials.new('SyntheticMixedSurface')
mat.use_nodes = True
obj.data.materials.append(mat)
bsdf = mat.node_tree.nodes.get('Principled BSDF')
mix = mat.node_tree.nodes.new('ShaderNodeMixRGB')
mix.blend_type = 'MULTIPLY'
mix.inputs[0].default_value = 1
mix.inputs[1].default_value = (.5,.25,.8,1)
mix.inputs[2].default_value = (.2,.4,.5,1)
mat.node_tree.links.new(mix.outputs[0], bsdf.inputs['Base Color'])
bsdf.inputs['Roughness'].default_value = .3
bsdf.inputs['Metallic'].default_value = .7
bsdf.inputs['Alpha'].default_value = .4
bsdf.inputs['Emission Color'].default_value = (.2,.1,.05,1)
bsdf.inputs['Emission Strength'].default_value = 10
result, report = bake_material(mat, 16)
for channel, expected in [('base',(.1,.1,.4,.4)), ('orm',(1,.3,.7,1)), ('emission',(1,.5,.25,1)), ('normal',(.5,.5,1,1))]:
    values = list(bpy.data.images[report['images'][channel]].pixels)
    actual = values[(8*16+8)*4:(8*16+8)*4+4]
    if channel in ('base','emission'):
        actual[:3]=[v/12.92 if v<=.04045 else ((v+.055)/1.055)**2.4 for v in actual[:3]]
    assert max(abs(a-b) for a,b in zip(actual,expected)) < .015, (channel,actual,expected)
assert obj.data.materials[0] == mat, 'Baking must not replace caller-owned assignments'
assert report['domain'] == 'UV0 unit square, periodically repeated'
assert report['emissionScale'] == 2
try:
    bake_material(mat,16,alpha_cutoff=float('nan'))
    raise AssertionError('Nonfinite alpha cutoff accepted')
except ValueError:
    pass
attribute = mat.node_tree.nodes.new('ShaderNodeAttribute')
mat.node_tree.links.new(attribute.outputs['Color'],bsdf.inputs['Base Color'])
try:
    bake_material(mat,16)
    raise AssertionError('Object attributes require mesh baking')
except ValueError:
    pass
# The exchange artifact must retain the same linear surface values after a
# normal glTF consumer decodes base color and emissive PNGs as sRGB.
if '--' in sys.argv:
    export_root=Path(sys.argv[sys.argv.index('--')+1])
    export_root.mkdir(parents=True,exist_ok=True)
    obj.data.materials[0]=result
    bpy.ops.object.select_all(action='DESELECT')
    obj.select_set(True)
    bpy.context.view_layer.objects.active=obj
    bpy.ops.export_scene.gltf(filepath=str(export_root/'surface.glb'),export_format='GLB',use_selection=True)
    (export_root/'expected.json').write_text(json.dumps({'base':[.1,.1,.4], 'alpha':.4,
        'emission':[2,1,.5], 'roughness':.3,'metallic':.7}),encoding='utf8')
print('PASS: evaluated mixed color, alpha, ORM and emission; original assignment preserved')
