"""Export a technical Blender candidate for isolated AzureRender validation."""
import argparse
import bpy,json,sys
from pathlib import Path
parser=argparse.ArgumentParser(description=__doc__)
parser.add_argument('--run-root',type=Path,required=True)
parser.add_argument('--clip',required=True)
args=parser.parse_args(sys.argv[sys.argv.index('--')+1:])
root=args.run_root.resolve(strict=True)
name=args.clip
folder=root/'animation-candidates'/name
if folder.resolve().parent != (root/'animation-candidates').resolve():raise ValueError('Invalid candidate path')
if (folder/'native-candidate.glb').exists():raise FileExistsError(folder/'native-candidate.glb')
bpy.ops.wm.open_mainfile(filepath=str(folder/'preview.blend'))
rig=next(o for o in bpy.context.scene.objects if o.type=='ARMATURE')
rig.animation_data.action.name=name
bpy.ops.object.select_all(action='DESELECT')
for obj in bpy.context.scene.objects:
    if obj.type=='ARMATURE' or (obj.type=='MESH' and not obj.hide_render):
        obj.hide_set(False);obj.select_set(True)
bpy.context.view_layer.objects.active=rig
bpy.context.scene.frame_set(1)
bpy.ops.export_scene.gltf(filepath=str(folder/'native-candidate.glb'),export_format='GLB',use_selection=True,export_animations=True,export_animation_mode='ACTIONS',export_force_sampling=True,export_frame_range=True,export_frame_step=1,export_yup=True,export_all_influences=True)
