"""Evaluate UV-periodic Principled inputs into portable glTF surface maps.

This adapter samples UV0 over the unit square. View-, object-, vertex- and
time-dependent shading requires a mesh-specific adapter, not this baker.
The original material and mesh assignments remain caller-owned.
"""
import bpy
import numpy as np


def bake_material(source, resolution=1024, alpha_cutoff=None):
    if resolution < 8 or resolution > 4096:
        raise ValueError('Bake resolution must be between 8 and 4096')
    if alpha_cutoff is not None and not 0 <= alpha_cutoff <= 1:
        raise ValueError('Alpha cutoff must be in the unit interval')
    if not source.use_nodes:
        raise ValueError('A node material is required')
    originals = [n for n in source.node_tree.nodes if n.type == 'BSDF_PRINCIPLED']
    if len(originals) != 1:
        raise ValueError('Exactly one Principled surface is required: ' + source.name)
    unsupported = {'TEX_COORD', 'ATTRIBUTE', 'VERTEX_COLOR', 'LAYER_WEIGHT', 'CAMERA', 'OBJECT_INFO', 'NEW_GEOMETRY'}
    for n in source.node_tree.nodes:
        if n.type in unsupported:
            active = [s.name for s in n.outputs if s.is_linked]
            if active and not (n.type == 'TEX_COORD' and active == ['UV']):
                raise ValueError('Non-UV material input requires mesh baking: ' + n.name)
    scene = bpy.context.scene
    scene.render.engine = 'CYCLES'
    scene.cycles.samples = 1
    scene.render.bake.margin = 0
    scene.render.bake.use_clear = True
    bpy.ops.object.select_all(action='DESELECT')
    bpy.ops.mesh.primitive_plane_add(size=2)
    plane = bpy.context.object
    temporary = source.copy()
    plane.data.materials.append(temporary)
    nodes, links = temporary.node_tree.nodes, temporary.node_tree.links
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
        bpy.ops.object.bake(type='NORMAL' if normal else 'EMIT')
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
        result['azureBakeDomain'] = 'UV0 unit square, periodically repeated'
        result.diffuse_color = source.diffuse_color
        result.surface_render_method = source.surface_render_method
        result.use_backface_culling = source.use_backface_culling
        rn, rl = result.node_tree.nodes, result.node_tree.links
        surface = rn.get('Principled BSDF')
        images = {}
        textures = {}
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
        rl.new(textures['normal'].outputs['Color'],normal.inputs['Color'])
        rl.new(normal.outputs[0],surface.inputs['Normal'])
        split = rn.new('ShaderNodeSeparateColor')
        rl.new(textures['orm'].outputs['Color'],split.inputs[0])
        rl.new(split.outputs[1],surface.inputs['Roughness'])
        rl.new(split.outputs[2],surface.inputs['Metallic'])
        rl.new(textures['emission'].outputs['Color'],surface.inputs['Emission Color'])
        surface.inputs['Emission Strength'].default_value = peak
        return result, dict(source=source.name,resolution=resolution,images=images,
                            emissionScale=peak,domain=result['azureBakeDomain'],
                            colorEncoding='sRGB RGB; linear alpha and data maps',storage='RGBA8 straight alpha')
    finally:
        mesh = plane.data
        bpy.data.objects.remove(plane,do_unlink=True)
        bpy.data.meshes.remove(mesh)
        bpy.data.materials.remove(temporary)
