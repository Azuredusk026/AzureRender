"""Evaluate UV-periodic Principled inputs into portable glTF surface maps.

The default domain is periodic UV0. An explicit mesh and target UV layer allow
independent named UV inputs. View-, object-, vertex- and time-dependent inputs
require a separate adapter. Source materials and meshes remain caller-owned.
"""
import bpy
import numpy as np


def bake_material(source, resolution=1024, alpha_cutoff=None, *, mesh=None, uv_layer=None):
    if resolution < 8 or resolution > 4096:
        raise ValueError('Bake resolution must be between 8 and 4096')
    if alpha_cutoff is not None and not 0 <= alpha_cutoff <= 1:
        raise ValueError('Alpha cutoff must be in the unit interval')
    if not source.use_nodes:
        raise ValueError('A node material is required')
    if (mesh is None) != (uv_layer is None):
        raise ValueError('Mesh baking requires an explicit mesh and UV target')
    if mesh is not None and (not mesh.polygons or not isinstance(uv_layer,str)
            or uv_layer not in mesh.uv_layers or any(p.material_index != 0 for p in mesh.polygons)):
        raise ValueError('Mesh baking requires one material and a valid UV target')
    originals = [n for n in source.node_tree.nodes if n.type == 'BSDF_PRINCIPLED']
    if len(originals) != 1:
        raise ValueError('Exactly one Principled surface is required: ' + source.name)
    unsupported = {'TEX_COORD', 'ATTRIBUTE', 'VERTEX_COLOR', 'LAYER_WEIGHT', 'CAMERA', 'OBJECT_INFO', 'NEW_GEOMETRY'}
    for n in source.node_tree.nodes:
        if n.type == 'UVMAP' and any(s.is_linked for s in n.outputs):
            if mesh is None or n.uv_map not in mesh.uv_layers:
                raise ValueError('Named UV maps require a matching mesh layer: ' + n.uv_map)
        if n.type in unsupported:
            active = [s.name for s in n.outputs if s.is_linked]
            if active and not (n.type == 'TEX_COORD' and active == ['UV']):
                raise ValueError('Non-UV material input requires mesh baking: ' + n.name)
    scene = bpy.context.scene
    scene.render.engine = 'CYCLES'
    scene.cycles.samples = 1
    scene.render.bake.margin = 8 if mesh is not None else 0
    if mesh is not None:scene.render.bake.margin_type='EXTEND'
    scene.render.bake.use_clear = True
    bpy.ops.object.select_all(action='DESELECT')
    if mesh is None:
        bpy.ops.mesh.primitive_plane_add(size=2)
        plane = bpy.context.object
    else:
        plane = bpy.data.objects.new(source.name+'-bake-target',mesh.copy())
        bpy.context.collection.objects.link(plane)
        plane.select_set(True)
        bpy.context.view_layer.objects.active=plane
        plane.data.materials.clear()
    temporary = source.copy()
    plane.data.materials.append(temporary)
    nodes, links = temporary.node_tree.nodes, temporary.node_tree.links
    if mesh is not None:
        primary=next((layer.name for layer in mesh.uv_layers if layer.active_render),mesh.uv_layers[0].name)
        primary_node=nodes.new('ShaderNodeUVMap');primary_node.uv_map=primary
        for node in list(nodes):
            if node.type == 'TEX_COORD':
                for link in list(node.outputs['UV'].links):
                    links.new(primary_node.outputs[0],link.to_socket)
            elif node.type == 'TEX_IMAGE' and not node.inputs['Vector'].is_linked:
                links.new(primary_node.outputs[0],node.inputs['Vector'])
            elif node.type == 'NORMAL_MAP' and not node.uv_map:
                node.uv_map=primary
        plane.data.uv_layers.active=plane.data.uv_layers[uv_layer]
        plane.data.uv_layers[uv_layer].active_render=True
    bsdf = next(n for n in nodes if n.type == 'BSDF_PRINCIPLED')
    output = next(n for n in nodes if n.type == 'OUTPUT_MATERIAL' and n.is_active_output)
    target = nodes.new('ShaderNodeTexImage')
    emitter = nodes.new('ShaderNodeEmission')
    arrays = {}

    def capture(name, socket=None, normal=False):
        image = bpy.data.images.new(source.name + '-' + name + '-working', width=resolution,
                                    height=resolution, alpha=True, float_buffer=True)
        image.colorspace_settings.name = 'Non-Color'
        target.image = image
        nodes.active = target
        if normal:
            links.new(bsdf.outputs[0], output.inputs['Surface'])
        else:
            for link in list(emitter.inputs['Color'].links):
                links.remove(link)
            if socket.is_linked:
                links.new(socket.links[0].from_socket, emitter.inputs['Color'])
            else:
                value = socket.default_value
                emitter.inputs['Color'].default_value = tuple(value) if socket.type == 'RGBA' else (value,value,value,1)
            links.new(emitter.outputs[0], output.inputs['Surface'])
        bpy.ops.object.bake(type='NORMAL' if normal else 'EMIT',
                            **({'uv_layer':uv_layer} if mesh is not None else {}))
        pixels = np.empty(resolution*resolution*4, dtype=np.float32)
        image.pixels.foreach_get(pixels)
        arrays[name] = pixels.reshape((-1,4))
        bpy.data.images.remove(image)

    try:
        capture('normal', normal=True)
        for name, socket in [('base','Base Color'), ('alpha','Alpha'), ('roughness','Roughness'),
                             ('metallic','Metallic'), ('emission','Emission Color'), ('strength','Emission Strength')]:
            capture(name, bsdf.inputs[socket])
        arrays['base'][:,3] = arrays['alpha'][:,0]
        arrays['orm'] = np.ones_like(arrays['base'])
        arrays['orm'][:,1] = arrays['roughness'][:,0]
        arrays['orm'][:,2] = arrays['metallic'][:,0]
        arrays['emission'][:,:3] *= arrays['strength'][:,0:1]
        peak = max(1.,float(arrays['emission'][:,:3].max()))
        arrays['emission'][:,:3] /= peak
        result = bpy.data.materials.new(source.name + '-baked')
        result.use_nodes = True
        for key in source.keys():
            result[key] = source[key]
        result['azureBakeDomain'] = ('UV0 unit square, periodically repeated' if mesh is None
                                    else 'mesh target UV layer: '+uv_layer)
        result.diffuse_color = source.diffuse_color
        result.surface_render_method = source.surface_render_method
        result.use_backface_culling = source.use_backface_culling
        rn, rl = result.node_tree.nodes, result.node_tree.links
        surface = rn.get('Principled BSDF')
        images = {}
        textures = {}
        output_uv=None
        if mesh is not None:
            output_uv=rn.new('ShaderNodeUVMap');output_uv.uv_map=uv_layer
        for name in ('base','normal','orm','emission'):
            # Generated float images pack their linear buffer directly into PNG.
            # glTF consumers decode color PNGs as sRGB, so explicitly encode the
            # bounded color lanes into a straight-alpha byte image first.
            image = bpy.data.images.new(source.name+'-'+name, width=resolution,height=resolution,alpha=True,float_buffer=False)
            image.colorspace_settings.name = 'sRGB' if name in ('base','emission') else 'Non-Color'
            image.alpha_mode = 'STRAIGHT'
            pixels = np.clip(arrays[name],0.,1.).copy()
            if name in ('base','emission'):
                rgb = pixels[:,:3]
                pixels[:,:3] = np.where(rgb<=.0031308,rgb*12.92,1.055*np.power(rgb,1/2.4)-.055)
            image.pixels.foreach_set(pixels.ravel())
            image.update()
            image.pack()
            images[name] = image.name
            tex = rn.new('ShaderNodeTexImage'); tex.image=image
            if output_uv is not None:rl.new(output_uv.outputs[0],tex.inputs['Vector'])
            textures[name] = tex
        rl.new(textures['base'].outputs['Color'],surface.inputs['Base Color'])
        source_alpha = originals[0].inputs['Alpha']
        if alpha_cutoff is not None:
            clip = rn.new('ShaderNodeMath')
            clip.operation = 'GREATER_THAN'
            clip.inputs[1].default_value = alpha_cutoff
            rl.new(textures['base'].outputs['Alpha'],clip.inputs[0])
            rl.new(clip.outputs[0],surface.inputs['Alpha'])
        elif source_alpha.is_linked or source_alpha.default_value < 1:
            rl.new(textures['base'].outputs['Alpha'],surface.inputs['Alpha'])
        normal = rn.new('ShaderNodeNormalMap')
        if mesh is not None:normal.uv_map=uv_layer
        rl.new(textures['normal'].outputs['Color'],normal.inputs['Color'])
        rl.new(normal.outputs[0],surface.inputs['Normal'])
        split = rn.new('ShaderNodeSeparateColor')
        rl.new(textures['orm'].outputs['Color'],split.inputs[0])
        rl.new(split.outputs[1],surface.inputs['Roughness'])
        rl.new(split.outputs[2],surface.inputs['Metallic'])
        rl.new(textures['emission'].outputs['Color'],surface.inputs['Emission Color'])
        surface.inputs['Emission Strength'].default_value = peak
        return result, dict(source=source.name,resolution=resolution,images=images,
                            paddingPixels=8 if mesh is not None else 0,
                            emissionScale=peak,domain=result['azureBakeDomain'],
                            colorEncoding='sRGB RGB; linear alpha and data maps',storage='RGBA8 straight alpha')
    finally:
        mesh = plane.data
        bpy.data.objects.remove(plane,do_unlink=True)
        bpy.data.meshes.remove(mesh)
        bpy.data.materials.remove(temporary)
