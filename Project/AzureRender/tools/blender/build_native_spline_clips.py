"""Build independent rigid-node GLBs from a source spline inventory and probe.

Run with Blender --background --python this_file -- followed by the arguments.
The extractor adapter is supplied explicitly and remains an external dependency.
"""
import argparse
import hashlib
import json
import math
from pathlib import Path
import struct
import sys

import bpy
from mathutils import Euler, Matrix, Vector


def proper_rigid_world_matrix(value, identity):
    """Reject transforms whose scale or handedness slot animation would lose."""
    error = 'Spline conversion requires a proper rigid world matrix: ' + str(identity)
    if (not isinstance(value, list) or len(value) != 4
            or any(not isinstance(row, list) or len(row) != 4 for row in value)
            or any(type(v) not in (int, float) or not math.isfinite(v)
                   for row in value for v in row)):
        raise ValueError(error)
    if any(abs(v - expected) > 1e-6 for v, expected in zip(value[3], (0, 0, 0, 1))):
        raise ValueError(error)
    for left in range(3):
        for right in range(3):
            dot = sum(value[row][left] * value[row][right] for row in range(3))
            if abs(dot - (1 if left == right else 0)) > 1e-5:
                raise ValueError(error)
    matrix = Matrix(value)
    if abs(matrix.to_3x3().determinant() - 1) > 1e-5:
        raise ValueError(error)
    return matrix


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    for name in ('probe', 'inventory', 'world', 'dependencies', 'extractor-src', 'output'):
        parser.add_argument('--' + name, type=Path, required=True)
    parser.add_argument('--source', required=True)
    parser.add_argument('--seed', type=int, required=True)
    parser.add_argument('--sample-rate', type=float, default=30)
    parser.add_argument('--resolution', type=int, default=1024)
    parser.add_argument('--surface-color-encoding', choices=('authored-srgb','gpu-linear'), default='authored-srgb')
    args = parser.parse_args(sys.argv[sys.argv.index('--') + 1:])
    tools = Path(__file__).resolve().parents[1]
    sys.path[:0] = [str(tools), str(tools / 'blender'), str(args.extractor_src)]
    from blender_build_map02_chunk_20260727 import import_obj
    from blender_scene_effects import _owner_matrix_root_relative, _blender_matrix
    from native_spline_materials import create_material
    from endfield_scene_material_contract import load_catalog, reference_key
    from bake_material_channels import bake_material
    from endfield_native.spline_bake import compile_emitters
    from convert_material_semantics import read_asset, write_asset

    def load(path):
        return json.loads(path.read_text(encoding='utf-8-sig'))

    manifest_path = args.probe / 'scene_manifest.json'
    inputs = [manifest_path, args.inventory, args.world, args.dependencies, Path(__file__),
              tools / 'endfield_native/spline_bake.py', tools / 'endfield_native/vfx_profile.py',
              tools / 'blender/native_spline_materials.py',
              tools / 'blender/native_surface_materials.py',
              tools / 'blender/bake_material_channels.py']
    inputs.extend(args.extractor_src / name for name in (
        'blender_scene_materials.py', 'blender_scene_effect_layers.py',
        'endfield_scene_material_contract.py', 'blender_scene_effects.py',
        'blender_build_map02_chunk_20260727.py'))
    input_hashes = {str(p.resolve()): hashlib.sha256(p.read_bytes()).hexdigest() for p in inputs}
    manifest = load(manifest_path)
    selected = [e for e in load(args.inventory)['emitters'] if e['identity'][0] == args.source]
    if not selected:
        raise ValueError('source selection contains no emitters')
    clips = compile_emitters(selected, seed=args.seed, sample_rate=args.sample_rate)
    worlds = {tuple(row['identity']): row for row in load(args.world)}
    world_matrices = {tuple(clip['identity']): proper_rigid_world_matrix(
        worlds[tuple(clip['identity'])]['candidate_world_matrix_row_major'], clip['identity'])
        for clip in clips}
    dependencies = {tuple(row['identity']): row for row in load(args.dependencies)['emitters']}
    nodes = {reference_key(row): {**row, 'Parent': {**row['Parent'],
             'Source': row['Parent']['Source'].lower()}} for row in manifest['Nodes']}
    owners = {reference_key(row['RendererAsset']): row for row in manifest['Nodes']
              if row.get('RendererAsset')}
    materials, textures = load_catalog((manifest_path,))
    meshes = {reference_key(row['Asset']): row for row in manifest['MeshExports']}
    args.output.mkdir(parents=True, exist_ok=False)
    bpy.context.scene.cycles.device = 'CPU'
    cache, reports = {}, []
    for clip in clips:
        identity = tuple(clip['identity'])
        worldrow = worlds[identity]
        world = world_matrices[identity]
        dep = dependencies[identity]
        bpy.ops.object.select_all(action='SELECT')
        bpy.ops.object.delete(use_global=False)
        templates, bakes, mesh_inputs = [], [], []
        for lod in dep['lod_groups']:
            group = next(row for row in manifest['LodGroups']
                         if reference_key(row['Asset']) == (lod['asset'][0].lower(), lod['asset'][1]))
            for draw in lod['levels'][0]['renderers']:
                mesh_record = meshes[reference_key(draw['mesh']['Asset'])] if 'Asset' in draw['mesh'] else draw['mesh']
                if mesh_record.get('VertexColorComponents', 0) != 0 or mesh_record.get('Uv1Components', 0) != 0:
                    raise ValueError('UV-only spline baking requires absent COLOR and UV1 source streams')
                mesh_path = args.probe / draw['mesh']['File'].replace('\\', '/')
                mesh_inputs.append({'identity': mesh_record['Asset'], 'path': str(mesh_path),
                                    'sha256': hashlib.sha256(mesh_path.read_bytes()).hexdigest()})
                objects = import_obj(mesh_path)
                owner = owners[reference_key(draw['identity'])]
                relative = _owner_matrix_root_relative(nodes, reference_key(group['GameObject']),
                                                       reference_key(owner))
                if len(objects) != len(owner['Materials']):
                    raise ValueError('source submesh/material count mismatch')
                for obj, ref in zip(objects, owner['Materials']):
                    key = reference_key(ref)
                    if not obj.data.uv_layers:
                        raise ValueError('UV-only spline baking requires source UV0')
                    outside = any(not 0 <= v <= 1 for uv in obj.data.uv_layers.active.data for v in uv.uv)
                    if outside:
                        rec = materials[key]
                        bound = [slot for slot in rec['Textures'] if slot.get('Texture')
                                 and reference_key(slot['Texture']) in textures]
                        periodic = rec['Shader']['Name'] == 'HGRP/LitEffect' and all(
                            slot['Texture'].get('TextureSampler', {}).get('WrapU') == 0
                            and slot['Texture'].get('TextureSampler', {}).get('WrapV') == 0
                            and all(math.isfinite(v) and v == round(v) for v in slot['Scale'])
                            for slot in bound)
                        if not periodic:
                            raise ValueError('UV0 outside the unit square requires verified periodic source sampling')
                    if key not in cache:
                        rec = materials[key]
                        original = create_material(rec, textures, True, color_encoding=args.surface_color_encoding)
                        cache[key] = bake_material(original, args.resolution,
                                                   original.get('endfield_alpha_clip_threshold'))
                    baked, report = cache[key]
                    texture_inputs = []
                    for slot in materials[key]['Textures']:
                        if slot.get('Texture') and reference_key(slot['Texture']) in textures:
                            texture_root, texture_file = textures[reference_key(slot['Texture'])]
                            texture_path = texture_root / texture_file
                            texture_inputs.append({'property': slot['Property'], 'path': str(texture_path),
                                'sha256': hashlib.sha256(texture_path.read_bytes()).hexdigest()})
                    bakes.append({'identity': list(key), 'shader': materials[key]['Shader'],
                                  'sourceMaterialSha256': hashlib.sha256(json.dumps(materials[key], sort_keys=True).encode()).hexdigest(),
                                  'textures': texture_inputs,
                                  'limitations': json.loads(baked.get('azureSourceLimitations', '[]')), **report})
                    obj.data.materials.clear()
                    obj.data.materials.append(baked)
                    templates.append((obj.data, _blender_matrix(relative)))
                    bpy.data.objects.remove(obj, do_unlink=True)
        roots = []
        for slot_index, slot in enumerate(clip['slots']):
            root = bpy.data.objects.new('source-slot-' + str(slot_index), None)
            bpy.context.collection.objects.link(root)
            rotation = Euler(tuple(math.radians(v) for v in slot['sourceEuler']), 'XYZ').to_matrix().to_4x4()
            matrix = world @ rotation
            matrix.translation = world @ Vector(slot['positions'][0])
            root.matrix_world = _blender_matrix(matrix)
            root.scale *= slot['scales'][0][0]
            roots.append(root.name)
            for draw_index, (mesh, relative) in enumerate(templates):
                obj = bpy.data.objects.new(f'source-draw-{slot_index}-{draw_index}', mesh)
                bpy.context.collection.objects.link(obj)
                obj.parent = root
                obj.matrix_basis = relative
        output = args.output / (clip['name'] + '.glb')
        bpy.ops.export_scene.gltf(filepath=str(output), export_format='GLB',
                                  export_animations=False, export_extras=True)
        document, tail = read_asset(output)
        blob = bytearray(tail[8:])
        lookup = {node.get('name'): i for i, node in enumerate(document['nodes'])}
        animation = {'name': 'source_spline_flow', 'samplers': [], 'channels': [],
                     'extensions': {'AZURE_node_opacity': {'schemaVersion': 1, 'channels': []}}}
        document.setdefault('extensionsUsed', []).append('AZURE_node_opacity')

        def accessor(values, width):
            blob.extend(b'\0' * (-len(blob) % 4))
            offset = len(blob)
            flat = [v for row in values for v in row]
            blob.extend(struct.pack('<' + 'f' * len(flat), *flat))
            view = len(document.setdefault('bufferViews', []))
            document['bufferViews'].append({'buffer': 0, 'byteOffset': offset,
                                             'byteLength': len(flat) * 4})
            entry = {'bufferView': view, 'componentType': 5126, 'count': len(values),
                     'type': {1: 'SCALAR', 3: 'VEC3'}[width]}
            if width == 1:
                entry.update(min=[min(flat)], max=[max(flat)])
            index = len(document.setdefault('accessors', []))
            document['accessors'].append(entry)
            return index

        for name, slot in zip(roots, clip['slots']):
            target = lookup[name]
            if 'matrix' in document['nodes'][target]:
                raise ValueError('animated slot exported as matrix node')
            times = accessor([[v] for v in slot['times']], 1)
            positions = [list(world @ Vector(point)) for point in slot['positions']]
            for path, values, interpolation in [('translation', positions, 'LINEAR'),
                                                 ('scale', slot['scales'], 'STEP')]:
                sampler = len(animation['samplers'])
                animation['samplers'].append({'input': times, 'output': accessor(values, 3),
                                               'interpolation': interpolation})
                animation['channels'].append({'sampler': sampler, 'target': {'node': target, 'path': path}})
            document['nodes'][target].setdefault('extensions', {})['AZURE_node_opacity'] = {
                'schemaVersion': 1, 'opacity': slot['opacities'][0]}
            sampler = len(animation['samplers'])
            animation['samplers'].append({'input': times,
                'output': accessor([[value] for value in slot['opacities']], 1), 'interpolation': 'LINEAR'})
            animation['extensions']['AZURE_node_opacity']['channels'].append({'sampler': sampler, 'node': target})
        document['animations'] = [animation]
        blob.extend(b'\0' * (-len(blob) % 4))
        document['buffers'] = [{'byteLength': len(blob)}]
        write_asset(output, document, struct.pack('<I4s', len(blob), b'BIN\0') + blob)
        reports.append({'identity': list(identity), 'name': clip['name'], 'file': output.name,
                        'slotCount': len(roots), 'period': clip['period'], 'timing': clip['timing'],
                        'channels': len(animation['channels']), 'sourceWorldTransform': worldrow,
                        'materials': bakes, 'meshes': mesh_inputs,
                        'outputSha256': hashlib.sha256(output.read_bytes()).hexdigest(),
                        'opacityChannels': len(roots), 'sourceAlphaApplied': True,
                        'nativeAlphaConsumerVerified': False, 'nativeActivationVerified': False,
                        'nativeGlobalPhaseVerified': False, 'nativeRandomVerified': False,
                        'rotationUnitsVerified': False})
        print('BUILT', clip['name'], len(roots), 'slots', flush=True)
    if input_hashes != {str(p.resolve()): hashlib.sha256(p.read_bytes()).hexdigest() for p in inputs}:
        raise ValueError('Conversion input changed during the build')
    receipt = {'format': 'NativeSplineGlbBatch/1', 'source': args.source, 'seed': args.seed,
               'sampleRate': args.sample_rate, 'blenderVersion': bpy.app.version_string, 'emitters': reports,
               'inputs': input_hashes,
               'limitations': ['candidate activation set and global phase',
                               'source Euler values interpreted as degrees',
                               'stable slot random samples; native resampling pending',
                               'source alpha curve mapped to generic node opacity; native consumer pending']}
    (args.output / 'receipt.json').write_text(json.dumps(receipt, indent=2), encoding='utf-8')


if __name__ == '__main__':
    main()
