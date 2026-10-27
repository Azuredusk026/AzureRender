"""Lit record-477 surface-algebra adapter for the pinned extractor.

The source extractor owns texture loading and UV sampling. This adapter owns
only the verified two-map tri-channel base/metallic/roughness arithmetic.
Source mesh UV selection and missing vertex-stream binding remain unverified.
"""
import bpy
from native_surface_colors import surface_color_record

from blender_scene_materials import create_material as _create_surface


_SHADER_SOURCE = 'cab-93483c2cb844b6b343af0950ad4630ad'
_SHADER_PATH_ID = 5324015590718682574
_PROGRAM_KEYWORDS = {'_TRI_CHANNEL_MASK', '_TWO_BASEMAP'}
_COMPILER_KEYWORDS = {'HG_ENABLE_MV', 'SRP_INSTANCING_ON'}


def _verified_branch(record):
    shader = record.get('Shader') or {}
    values = record.get('Floats') or {}
    keywords = set((record.get('RenderState') or {}).get('ValidKeywords', []))
    return (str(shader.get('Source', '')).casefold() == _SHADER_SOURCE
            and shader.get('PathId') == _SHADER_PATH_ID
            and values.get('_EnableTriChannelMask', 0) > .5
            and values.get('_BaseTextureMapCount') in (1, 2)
            and keywords - _COMPILER_KEYWORDS == _PROGRAM_KEYWORDS)


def _connect(tree, socket, value):
    if isinstance(value, bpy.types.NodeSocket):
        tree.links.new(value, socket)
    else:
        socket.default_value = value


def _math(tree, operation, *values, clamp=False, label=''):
    node = tree.nodes.new('ShaderNodeMath')
    node.operation, node.use_clamp, node.label = operation, clamp, label
    for socket, value in zip(node.inputs, values):
        _connect(tree, socket, value)
    return node.outputs[0]


def _vector(tree, operation, a, b):
    node = tree.nodes.new('ShaderNodeVectorMath')
    node.operation = operation
    _connect(tree, node.inputs[0], a)
    _connect(tree, node.inputs['Scale'] if operation == 'SCALE' else node.inputs[1], b)
    return node.outputs['Vector']


def _vector_lerp(tree, current, target, weight):
    difference = _vector(tree, 'SUBTRACT', target, current)
    return _vector(tree, 'ADD', current, _vector(tree, 'SCALE', difference, weight))


def _scalar_lerp(tree, current, target, weight):
    difference = _math(tree, 'SUBTRACT', target, current)
    return _math(tree, 'MULTIPLY_ADD', difference, weight, current)


def _texture(material, prop):
    matches = [node for node in material.node_tree.nodes
               if node.type == 'TEX_IMAGE' and node.label == prop]
    if len(matches) != 1:
        raise ValueError(f'Verified tri-channel surface requires one {prop}: {material.name}')
    return matches[0]


def create_material(record, texture_files, pack_textures=True, color_encoding='gpu-linear'):
    if color_encoding not in ('gpu-linear','authored-srgb'):
        raise ValueError('Unsupported surface color encoding')
    verified=_verified_branch(record)
    if verified:record=surface_color_record(record,color_encoding)
    material = _create_surface(record, texture_files, pack_textures)
    if not verified:
        return material
    values, colors = record.get('Floats') or {}, record.get('Colors') or {}
    if values.get('_UseMaskRUVSetAndTilingScale', 0) > .5:
        raise NotImplementedError('Independent R-mask sampling needs mesh UV-domain validation')
    tree = material.node_tree
    bsdf = next(node for node in tree.nodes if node.type == 'BSDF_PRINCIPLED')
    base, nro, mask = (_texture(material, prop) for prop in
                       ('_BaseColorMap', '_NormalMap', '_MaskMap'))

    # Record 477, base-source block: tint * brightness, saturate, then cover.
    base_tint = colors.get('_BaseColor', [1, 1, 1, 1])
    tinted = _vector(tree, 'MULTIPLY', base.outputs['Color'], base_tint[:3])
    brighter = _vector(tree, 'SCALE', tinted, values.get('_BaseColorBrighterScale', 1))
    split_base = tree.nodes.new('ShaderNodeSeparateXYZ')
    tree.links.new(brighter, split_base.inputs[0])
    saturated = tree.nodes.new('ShaderNodeCombineXYZ')
    for source, destination in zip(split_base.outputs, saturated.inputs):
        tree.links.new(_math(tree, 'MULTIPLY', source, 1, clamp=True), destination)
    current_base = _vector_lerp(tree, saturated.outputs[0], base_tint[:3],
                                values.get('_BaseColorTintCover', 0))

    split_nro = tree.nodes.new('ShaderNodeSeparateColor')
    tree.links.new(nro.outputs['Color'], split_nro.inputs['Color'])
    minimum = values.get('_RoughnessMin', 0)
    current_rough = _math(tree, 'MULTIPLY_ADD', split_nro.outputs['Blue'],
                          values.get('_RoughnessMax', 1) - minimum, minimum)
    # _TWO_BASEMAP mode 1 takes BaseColorMap alpha; mode 2 takes _Metallic.
    current_metal = (base.outputs['Alpha'] if values['_BaseTextureMapCount'] == 1
                     else float(values.get('_Metallic', 0)))
    split_mask = tree.nodes.new('ShaderNodeSeparateColor')
    tree.links.new(mask.outputs['Color'], split_mask.inputs['Color'])
    defaults = {'R': [1, 0, 0, 1], 'G': [0, 1, 0, 1], 'B': [0, 0, 1, 1]}
    for letter, channel in (('B', 'Blue'), ('G', 'Green'), ('R', 'Red')):
        scale = float(values.get(f'_Mask{letter}Scale', 0))
        offset = float(values.get(f'_Mask{letter}Offset', 0))
        tint = colors.get(f'_MaskAlbedo{letter}', defaults[letter])
        shifted = _math(tree, 'ADD', split_mask.outputs[channel], offset)
        remapped = _math(tree, 'MULTIPLY_ADD', shifted, 1 + scale, -.5 * scale,
                         clamp=True, label=f'{letter}: saturate((mask+offset)*(1+scale)-.5*scale)')
        weight = _math(tree, 'MULTIPLY', remapped, float(tint[3]),
                       label=f'{letter}: shared surface weight after saturation')
        current_base = _vector_lerp(tree, current_base, tint[:3], weight)
        current_rough = _scalar_lerp(tree, current_rough,
                                     values.get(f'_MaskRoghness{letter}', .25), weight)
        current_metal = _scalar_lerp(tree, current_metal,
                                     values.get(f'_MaskMetallic{letter}', 0), weight)
    for source, name in ((current_base, 'Base Color'), (current_rough, 'Roughness'),
                         (current_metal, 'Metallic')):
        tree.links.new(source, bsdf.inputs[name])
    material['endfield_tri_channel_mask_reconstruction'] = (
        'Lit record 477: B/G/R absolute base, roughness, metallic interpolation; '
        'shared saturate-adjusted mask times authored albedo alpha')
    material['endfield_tri_channel_contract'] = 'NativeTriSurfaceAlgebra/1'
    material['endfield_input_surface_color_encoding'] = color_encoding
    material['endfield_surface_color_encoding'] = 'gpu-linear'
    material['endfield_tri_channel_status'] = 'source_algebra_uv_domain_pending'
    material['endfield_tri_channel_uv_status'] = 'extractor_sampling_preserved_mesh_uv_and_runtime_binding_pending'
    material['endfield_surface_fidelity'] = 'source_tri_surface_algebra_mesh_uv_runtime_and_lighting_pending'
    return material
