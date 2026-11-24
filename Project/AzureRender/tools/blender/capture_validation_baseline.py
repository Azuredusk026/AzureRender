"""Capture immutable Blender import baselines in an isolated factory session."""
import argparse
import hashlib
import json
import sys
from pathlib import Path
import bpy
from mathutils import Vector


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument('--input', required=True, type=Path)
    parser.add_argument('--output', required=True, type=Path)
    parser.add_argument('--action')
    args = parser.parse_args(sys.argv[sys.argv.index('--') + 1:])
    args.output.mkdir(parents=True, exist_ok=True)
    bpy.ops.wm.read_factory_settings(use_empty=True)
    bpy.ops.import_scene.gltf(filepath=str(args.input.resolve()))
    meshes = [o for o in bpy.context.scene.objects if o.type == 'MESH']
    armatures = [o for o in bpy.context.scene.objects if o.type == 'ARMATURE']
    actions = list(bpy.data.actions)
    chosen = next((a for a in actions if a.name == args.action or a.name.endswith('_' + str(args.action))), None) if args.action else (actions[0] if actions else None)
    if args.action and chosen is None:
        raise ValueError('Requested action missing: ' + str([a.name for a in actions]))
    for obj in armatures:
        obj.animation_data_create()
        for track in obj.animation_data.nla_tracks: track.mute = True
        if chosen:
            obj.animation_data.action = chosen
            if chosen.slots: obj.animation_data.action_slot = chosen.slots[0]
    scene = bpy.context.scene
    scene.render.engine = 'BLENDER_WORKBENCH'
    scene.render.resolution_x = 640
    scene.render.resolution_y = 640
    scene.render.resolution_percentage = 100
    scene.display.shading.light = 'STUDIO'
    scene.display.shading.color_type = 'MATERIAL'
    scene.display.shading.show_shadows = True
    scene.display.shading.show_cavity = True
    scene.display.shading.background_type = 'WORLD'
    scene.world = bpy.data.worlds.new('BaselineWorld')
    scene.world.color = (0.08, 0.08, 0.08)
    start, end = list(chosen.frame_range) if chosen else (0, 0)
    scene.frame_set(int(start))
    points = [obj.matrix_world @ Vector(c) for obj in meshes for c in obj.bound_box]
    lower = Vector([min(v[i] for v in points) for i in range(3)])
    upper = Vector([max(v[i] for v in points) for i in range(3)])
    center = (lower + upper) / 2
    size = max(upper - lower)
    bpy.ops.object.camera_add(location=center + Vector((0, -size * 2, size * .12)))
    camera = bpy.context.object
    camera.rotation_euler = (center - camera.location).to_track_quat('-Z', 'Y').to_euler()
    camera.data.type = 'ORTHO'
    camera.data.ortho_scale = size * 1.15
    scene.camera = camera
    frames = []
    for i in range(5):
        frame = start + (end - start) * i / 4
        scene.frame_set(int(frame), subframe=frame % 1)
        scene.render.filepath = str((args.output / f'frame-{i}.png').resolve())
        bpy.ops.render.render(write_still=True)
        frames.append(dict(frame=frame, timeSeconds=(frame-start)/scene.render.fps, image=f'frame-{i}.png', bones={obj.name: {bone.name: [list(row) for row in obj.matrix_world @ bone.matrix] for bone in obj.pose.bones} for obj in armatures}))
    report = dict(schemaVersion=1, blenderVersion=bpy.app.version_string, sourceSha256=hashlib.sha256(args.input.read_bytes()).hexdigest(), source=str(args.input.resolve()), engine=scene.render.engine, appearanceScope='material-colour geometry baseline; game shader evaluation excluded', camera=dict(location=list(camera.location), rotation=list(camera.rotation_euler), orthographicScale=camera.data.ortho_scale), fps=scene.render.fps, coordinateSpace='Blender world Z-up meters', actions=[a.name for a in actions], chosenAction=chosen.name if chosen else None, meshes=len(meshes), bones=sum(len(o.data.bones) for o in armatures), materials=len(bpy.data.materials), bounds=[list(lower),list(upper)], samples=frames)
    (args.output / 'baseline.json').write_text(json.dumps(report, ensure_ascii=False, indent=2), encoding='utf-8')

if __name__ == '__main__': main()
