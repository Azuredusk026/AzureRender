"""Run with Blender --background --factory-startup --python this_file."""
import sys
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
bsdf.inputs['Emission Strength'].default_value = 2
result, report = bake_material(mat, 16)
for channel, expected in [('base',(.1,.1,.4,.4)), ('orm',(1,.3,.7,1)), ('emission',(.4,.2,.1,1)), ('normal',(.5,.5,1,1))]:
    values = list(bpy.data.images[report['images'][channel]].pixels)
    actual = values[(8*16+8)*4:(8*16+8)*4+4]
    assert max(abs(a-b) for a,b in zip(actual,expected)) < .015, (channel,actual,expected)
assert obj.data.materials[0] == mat, 'Baking must not replace caller-owned assignments'
assert report['domain'] == 'UV0 unit square, periodically repeated'
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
print('PASS: evaluated mixed color, alpha, ORM and emission; original assignment preserved')
