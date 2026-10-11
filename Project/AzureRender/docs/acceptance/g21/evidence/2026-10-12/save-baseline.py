from pathlib import Path
import json,subprocess
root=Path.cwd();out=root/'build/g21-dependency-evaluation-2026-10-12'
names=['CMakeLists.txt','vcpkg.json','README.md','CHANGELOG.md','docs/plans/azure-engine-plan.md','src/assets/AssetTransfer.cpp','src/assets/AssetTransfer.hpp','src/assets/AssetFormatRegistry.hpp','src/assets/AssetFormatRegistry.cpp','src/assets/model/ModelFormatAdapter.cpp','src/assets/model/ModelFormatAdapter.hpp','src/assets/image/ImageFormatAdapter.cpp','src/assets/audio/AudioFormatAdapter.cpp','tests/EditorAssetWorkflowTests.cpp']
document={name:{'head':subprocess.check_output(['git','show','HEAD:Project/AzureRender/'+name]).decode(),'work':(root/name).read_text(encoding='utf-8')} for name in names}
(out/'stage-baseline.json').write_text(json.dumps(document)+'\n',encoding='utf-8');print('Saved G21 HEAD and working-tree baseline')
