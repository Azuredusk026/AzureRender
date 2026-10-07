"""Export source skeletal weights through Unreal's FBX exporter without saving assets."""
import os
from pathlib import Path
import unreal

source=os.environ['AZURE_SOURCE_SKELETAL_MESH']
output=Path(os.environ['AZURE_SOURCE_FBX_OUTPUT'])
if output.exists():raise FileExistsError(output)
asset=unreal.load_asset(source)
if asset is None:raise RuntimeError('Source skeletal mesh missing: '+source)
output.parent.mkdir(parents=True,exist_ok=True)
task=unreal.AssetExportTask();task.object=asset;task.filename=str(output)
task.automated=True;task.prompt=False;task.replace_identical=False
options=unreal.FbxExportOption();options.ascii=False;options.level_of_detail=False
options.export_morph_targets=False;options.export_preview_mesh=False;task.options=options
if not unreal.Exporter.run_asset_export_task(task):raise RuntimeError('Source FBX export failed')
if not output.is_file():raise RuntimeError('Source FBX file missing')
unreal.log('AZURE_SOURCE_FBX_SUCCESS '+str(output))
