"""Original low-cost environment meshes with explicit glTF PBR textures."""
import bpy,json,math,sys
from pathlib import Path

folder=Path(sys.argv[sys.argv.index('--')+1]);folder.mkdir(parents=True,exist_ok=True)
bpy.ops.object.select_all(action='SELECT');bpy.ops.object.delete(use_global=False)

def material(name):
    m=bpy.data.materials.new(name);m.use_nodes=True
    nodes=m.node_tree.nodes;links=m.node_tree.links;bsdf=nodes.get('Principled BSDF')
    def image(suffix,linear=False):
        node=nodes.new('ShaderNodeTexImage');node.image=bpy.data.images.load(str(folder/f'{name}-{suffix}.png'))
        node.image.colorspace_settings.name='Non-Color' if linear else 'sRGB';return node
    links.new(image('albedo').outputs['Color'],bsdf.inputs['Base Color'])
    normal=nodes.new('ShaderNodeNormalMap');normal.inputs['Strength'].default_value=.55
    links.new(image('normal',True).outputs['Color'],normal.inputs['Color']);links.new(normal.outputs['Normal'],bsdf.inputs['Normal'])
    packed=image('orm',True);channels=nodes.new('ShaderNodeSeparateColor')
    links.new(packed.outputs['Color'],channels.inputs['Color'])
    links.new(channels.outputs['Green'],bsdf.inputs['Roughness']);links.new(channels.outputs['Blue'],bsdf.inputs['Metallic'])
    return m

materials={name:material(name) for name in ['stone','sand','leaf','relic']}

def cube(name,dimensions,position,mat='stone'):
    bpy.ops.mesh.primitive_cube_add(size=2,location=position);obj=bpy.context.object;obj.name=name
    obj.scale=[d/2 for d in dimensions];bpy.ops.object.transform_apply(location=False,rotation=False,scale=True)
    obj.data.materials.append(materials[mat])
    # World-size UVs give one texture tile per two metres on every face.
    uv=obj.data.uv_layers.active.data
    for polygon in obj.data.polygons:
        axis=max(range(3),key=lambda i:abs(polygon.normal[i]));a,b=[i for i in range(3) if i!=axis]
        for loop in polygon.loop_indices:
            vertex=obj.data.vertices[obj.data.loops[loop].vertex_index].co
            uv[loop].uv=(vertex[a]/2,vertex[b]/2)
    return obj

def export(name,objects):
    bpy.ops.object.select_all(action='DESELECT')
    for obj in objects:obj.select_set(True)
    bpy.context.view_layer.objects.active=objects[0]
    for obj in objects:obj['license']='CC0-1.0';obj['generator']='azure.exploration-environment/1'
    bpy.ops.export_scene.gltf(filepath=str(folder/f'{name}.glb'),export_format='GLB',use_selection=True,
        export_tangents=True,export_extras=True,export_animations=False)
    for obj in objects:bpy.data.objects.remove(obj,do_unlink=True)

# Blender Z-up becomes glTF Y-up at export.
export('ground',[cube('ground',(24,116,.5),(0,0,0),'stone')])
export('trail',[cube('trail',(4,110,.02),(0,0,0),'sand')])
export('branch',[cube('branch',(6,2,.02),(0,0,0),'sand')])
bpy.ops.mesh.primitive_ico_sphere_add(subdivisions=2,radius=1)
rock=bpy.context.object;rock.name='weathered-rock';rock.scale=(1.25,.85,.95)
bpy.ops.object.transform_apply(location=False,rotation=False,scale=True)
for vertex in rock.data.vertices:
    vertex.co*=1+.08*math.sin(vertex.co.x*8+vertex.co.y*5)
minimum=min(v.co.z for v in rock.data.vertices)
for vertex in rock.data.vertices:vertex.co.z-=minimum
bpy.ops.object.mode_set(mode='EDIT');bpy.ops.mesh.select_all(action='SELECT');bpy.ops.uv.smart_project();bpy.ops.object.mode_set(mode='OBJECT')
rock.data.materials.append(materials['stone']);export('rock',[rock])
plants=[]
for index,(x,y,z,radius) in enumerate([(-.25,0,.45,.5),(.3,.12,.65,.55),(0,-.18,.9,.6)]):
    bpy.ops.mesh.primitive_ico_sphere_add(subdivisions=2,radius=radius,location=(x,y,z))
    plant=bpy.context.object;plant.name='shrub-'+str(index);plant.scale=(1,.7,1)
    bpy.ops.object.mode_set(mode='EDIT');bpy.ops.mesh.select_all(action='SELECT');bpy.ops.uv.smart_project();bpy.ops.object.mode_set(mode='OBJECT')
    for polygon in plant.data.polygons:polygon.use_smooth=True
    plant.data.materials.append(materials['leaf']);plants.append(plant)
export('shrub',plants)
export('ruin',[cube('column-base',(1.6,1.6,.35),(0,0,.175)),cube('column',(1,1,3.4),(0,0,2.05)),cube('column-cap',(1.5,1.5,.45),(0,0,3.975))])
export('arch',[cube('left-pier',(1,1.2,4.5),(-3.2,0,2.25)),cube('right-pier',(1,1.2,4.5),(3.2,0,2.25)),cube('lintel',(7.4,1.2,.9),(0,0,4.5),'sand')])
export('relic',[cube('golden-relic',(1.4,1.4,1.7),(0,0,0),'relic')])
(folder/'blender-version.txt').write_text(bpy.app.version_string+'\n',encoding='utf-8')
