from pathlib import Path
import sys,time,json,ctypes
root=Path.cwd();sys.path.insert(0,str(root/'tools'))
source=(root/'build/p4/native_framing_source.py').read_text(encoding='utf-8')
namespace={'__file__':str(root/'tools/test_editor_native_input.py'),'__name__':'native_framing'}
exec(compile(source,namespace['__file__'],'exec'),namespace)
assert ctypes.WinDLL('user32').SetProcessDPIAware()
output=root/'build/p4/native-framing'/('run-'+str(time.time_ns()))
rows=[namespace['run'](root/'build/ninja-msvc-release/AzureRender.exe',output/workflow,workflow) for workflow in ('exploration','inspector')]
(output/'summary.json').write_text(json.dumps(rows,indent=2),encoding='utf-8');print(json.dumps(rows))
