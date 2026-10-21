"""UV-only spline material adapter for the explicitly supplied extractor.

The external extractor stays read-only. Its VFX module is loaded into a private
namespace so per-axis sampler support cannot alter unrelated Blender imports.
Static UV snapshots do not reconstruct depth soft blending or shader animation.
"""
import importlib.util
import json

import bpy

import blender_scene_effect_layers as _effects_source
from blender_scene_materials import create_material as _create_surface, texture_image
from endfield_scene_material_contract import reference_key, resolve_export_path, validate_texture_sampler


def _math(tree, operation, a, b):
    node = tree.nodes.new('ShaderNodeMath')
    node.operation = operation
    for socket, value in zip(node.inputs, (a, b)):
        if isinstance(value, (int, float)):
            socket.default_value = value
        else:
            tree.links.new(value, socket)
    return node.outputs[0]


def _effect_adapter():
    spec = importlib.util.spec_from_file_location('_azure_spline_effects', _effects_source.__file__)
    module = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(module)
    original_settings, original_sample = module._sampler_settings, module._sample

    def settings(record, name, reference):
        result = original_settings(record, name, reference)
        sampler = reference.get('TextureSampler')
        if sampler and (sampler['WrapU'], sampler['WrapV']) in ((0, 1), (1, 0)):
            symmetric = dict(reference, TextureSampler=dict(sampler, WrapU=0, WrapV=0))
            return original_settings(record, name, symmetric)
        return result

    def sample(tree, record, name, textures, uv):
        reference = next(s['Texture'] for s in record['Textures'] if s['Property'] == name)
        sampler = reference.get('TextureSampler') if reference else None
        if sampler and (sampler['WrapU'], sampler['WrapV']) in ((0, 1), (1, 0)):
            validate_texture_sampler(sampler, f'{record["Name"]}:{name}')
            root, filename = textures[reference_key(reference)]
            image = texture_image(resolve_export_path(root / filename),
                                  'Non-Color' if reference['ColorSpace'] == 0 else 'sRGB', 'STRAIGHT')
            split = tree.nodes.new('ShaderNodeSeparateXYZ')
            combine = tree.nodes.new('ShaderNodeCombineXYZ')
            tree.links.new(uv, split.inputs[0])
            for axis, wrap, dimension in zip(('X', 'Y'), (sampler['WrapU'], sampler['WrapV']), image.size):
                component = split.outputs[axis]
                if wrap == 1:
                    # Clamp at the edge texel centers while REPEAT preserves
                    # correct bilinear filtering across the other axis seam.
                    component = _math(tree, 'MAXIMUM', component, .5 / dimension)
                    component = _math(tree, 'MINIMUM', component, 1 - .5 / dimension)
                tree.links.new(component, combine.inputs[axis])
            uv = combine.outputs[0]
        return original_sample(tree, record, name, textures, uv)

    module._sampler_settings, module._sample = settings, sample
    return module


_effects = _effect_adapter()


def _static_emission(material, record):
    tree = material.node_tree
    surface = next(n for n in tree.nodes if n.type == 'BSDF_PRINCIPLED')
    texture = next(n for n in tree.nodes if n.type == 'TEX_IMAGE' and n.label == '_EmissiveMap')
    floats, colors = record['Floats'], record['Colors']
    if floats['_EmissiveType'] not in (0, 1):
        raise ValueError('Unsupported emissive channel selector')
    if floats.get('_EmissiveMapTilling', 1) != 1:
        raise ValueError('Static LitEffect adapter requires unit emissive tiling')
    split = tree.nodes.new('ShaderNodeSeparateColor')
    tree.links.new(texture.outputs['Color'], split.inputs[0])
    contributions = [('Red', '_EmissiveColor')]
    if floats['_EmissiveType'] == 1:
        contributions += [('Green', '_EmissiveColorG'), ('Blue', '_EmissiveColorB'), ('Alpha', '_EmissiveColorA')]
    value = None
    for channel, parameter in contributions:
        tint = tree.nodes.new('ShaderNodeMixRGB')
        tint.blend_type, tint.use_clamp = 'MULTIPLY', False
        tint.inputs[0].default_value = 1
        tint.inputs[2].default_value = colors[parameter]
        tree.links.new(texture.outputs['Alpha'] if channel == 'Alpha' else split.outputs[channel], tint.inputs[1])
        if value is None:
            value = tint.outputs[0]
        else:
            add = tree.nodes.new('ShaderNodeMixRGB')
            add.blend_type, add.use_clamp = 'ADD', False
            add.inputs[0].default_value = 1
            tree.links.new(value, add.inputs[1])
            tree.links.new(tint.outputs[0], add.inputs[2])
            value = add.outputs[0]
    if floats.get('_AlbedoAffectEmissive', 1) == 0:
        albedo = surface.inputs['Base Color']
        multiply = tree.nodes.new('ShaderNodeMixRGB')
        multiply.blend_type = 'MULTIPLY'
        multiply.inputs[0].default_value = 1
        tree.links.new(value, multiply.inputs[1])
        if albedo.is_linked:
            tree.links.new(albedo.links[0].from_socket, multiply.inputs[2])
        else:
            multiply.inputs[2].default_value = albedo.default_value
        value = multiply.outputs[0]
    tree.links.new(value, surface.inputs['Emission Color'])
    surface.inputs['Emission Strength'].default_value = 1
    material['azureSourceLimitations'] = json.dumps([
        'Static LitEffect RGBA emission; compiled intensity/remap and runtime UV motion pending'])


def create_material(record, textures, pack_textures=True):
    shader = record['Shader']['Name']
    if shader != 'HGRP/Effect/VFXBaseV2':
        material = _create_surface(record, textures, pack_textures)
        if shader == 'HGRP/LitEffect' and record['Floats'].get('_EnableEmissiveMap') == 1:
            _static_emission(material, record)
        return material
    floats = record['Floats']
    expected = {'_SurfaceType': 1, '_SrcBlend': 1, '_DstBlend': 10, '_BlendMode': 0, '_ZWrite': 0}
    if any(floats.get(k) != v for k, v in expected.items()):
        raise ValueError('VFX blend/depth state requires a separate adapter: ' + record['Name'])
    material = bpy.data.materials.new(record['Name'])
    material.use_nodes = True
    material.surface_render_method = 'BLENDED'
    material.use_backface_culling = floats.get('_CullMode') == 2
    tree = material.node_tree
    source = _effects.make_vfx_mesh_source(tree, record, textures, 0, 0)
    surface = tree.nodes.get('Principled BSDF')
    surface.inputs['Base Color'].default_value = (0, 0, 0, 1)
    surface.inputs['Roughness'].default_value = 1
    surface.inputs['Specular IOR Level'].default_value = 0
    rgb = source.rgb
    if floats.get('_ExpIntensity', 0) != 0:
        excess = tree.nodes.new('ShaderNodeVectorMath')
        excess.operation = 'SUBTRACT'
        tree.links.new(rgb, excess.inputs[0])
        excess.inputs[1].default_value = (floats['_ExpThreshold'],) * 3
        positive = tree.nodes.new('ShaderNodeVectorMath')
        positive.operation = 'MAXIMUM'
        tree.links.new(excess.outputs[0], positive.inputs[0])
        positive.inputs[1].default_value = (0, 0, 0)
        gain = tree.nodes.new('ShaderNodeVectorMath')
        gain.operation = 'SCALE'
        tree.links.new(positive.outputs[0], gain.inputs[0])
        gain.inputs['Scale'].default_value = floats['_ExpIntensity']
        add = tree.nodes.new('ShaderNodeVectorMath')
        add.operation = 'ADD'
        tree.links.new(rgb, add.inputs[0])
        tree.links.new(gain.outputs[0], add.inputs[1])
        rgb = add.outputs[0]
    tree.links.new(rgb, surface.inputs['Emission Color'])
    surface.inputs['Emission Strength'].default_value = 1
    tree.links.new(_math(tree, 'MINIMUM', _math(tree, 'MAXIMUM', source.alpha, 0), 1), surface.inputs['Alpha'])
    material['azureSourceLimitations'] = json.dumps([
        *source.limitations, 'Static UV snapshot; runtime exposure and depth soft intersection pending'])
    return material
