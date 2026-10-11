from pathlib import Path
import difflib,json,subprocess
root=Path.cwd();out=root/'build/g19-dependency-evaluation-2026-10-12'
baseline=json.loads((out/'stage-baseline.json').read_text(encoding='utf-8'))
before=subprocess.check_output(['git','diff','--cached','--name-only']).decode();assert not before,before
name='CMakeLists.txt';old=baseline[name]['work'];new=(root/name).read_text(encoding='utf-8')
patch=''.join(difflib.unified_diff(old.splitlines(True),new.splitlines(True),fromfile='a/Project/AzureRender/'+name,tofile='b/Project/AzureRender/'+name))
subprocess.run(['git','-C',str(root.parents[1]),'apply','--cached','--check','-'],input=patch.encode(),check=True)
subprocess.run(['git','-C',str(root.parents[1]),'apply','--cached','-'],input=patch.encode(),check=True)
files=[name for name,values in baseline.items() if name!='CMakeLists.txt' and values['head']==values['work']]
files+=['src/assets/image/ImageCodec.hpp','src/assets/image/ImageCodec.cpp','src/assets/image/ImageFormatAdapter.cpp','src/editor/commands/EditorOperations.cpp','tests/ImageCodecTests.cpp','tests/EditorAssetWorkflowTests.cpp','tests/TextureMaterialImportTests.cpp','docs/runtime/image-format-import.md','docs/licenses/image-codecs-source.json']
subprocess.run(['git','add','--',*files],check=True)
print('Only G19 source increments are staged')
