import re
from pathlib import Path
run=Path(__file__).resolve().parent;p=run/'build_green.py';text=p.read_text();assert "'RenderSettingsTests'" in text;text=text.replace("'RenderSettingsTests'","'EnvironmentSpecularTests'");p.write_bytes(text.encode());cmake=(Path.cwd()/'CMakeLists.txt').read_text();names=re.search(r'names=(\[.*?\]);text=',text).group(1);import ast
for name in ast.literal_eval(names):assert re.search(r'add_executable\(\s*(\w+)\s+tests/'+name+r'\.cpp',cmake),name
print('Validated all 17 actual CMake test targets')
