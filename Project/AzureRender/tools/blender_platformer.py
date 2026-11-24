"""Generate original beveled platforms and a gold coin. CC0-1.0."""
import bpy,math,sys
from pathlib import Path
out=Path(sys.argv[sys.argv.index('--')+1]);out.mkdir(parents=True,exist_ok=True)
def material(name,color,metal=0,rough=.7):
    m=bpy.data.materials.new(name);m.use_nodes=True;p=m.node_tree.nodes.get('Principled BSDF');p.inputs['Base Color'].default_value=(*color,1);p.inputs['Metallic'].default_value=metal;p.inputs['Roughness'].default_value=rough;return m
def export(name):
    bpy.ops.object.transform_apply(location=False,rotation=True,scale=True)
    bpy.ops.export_scene.gltf(filepath=str(out/(name+'.glb')),export_format='GLB',use_selection=True)
    bpy.ops.object.delete()
bpy.ops.object.select_all(action='SELECT');bpy.ops.object.delete()
for name,color in [('platform',(.18,.28,.32)),('practice',(.25,.42,.35)),('moving',(.17,.44,.65)),('challenge',(.5,.29,.15))]:
    bpy.ops.mesh.primitive_cube_add();o=bpy.context.object;o.data.materials.append(material(name,color));b=o.modifiers.new('Rounded edge','BEVEL');b.width=.045;b.segments=2;bpy.ops.object.modifier_apply(modifier=b.name);export(name)
bpy.ops.mesh.primitive_cylinder_add(vertices=24,radius=.33,depth=.09,rotation=(math.pi/2,0,0));bpy.context.object.data.materials.append(material('gold',(.95,.61,.08),.75,.27));export('coin')
(out/'blender-version.txt').write_text(bpy.app.version_string+'\n')
