import json
from pathlib import Path
import sys
sys.path.insert(0, str(Path('tools').resolve()))
from trace_virtual_allocations import trace

output=Path(sys.argv[1])
expected=['C:\\Source\\path with spaces\\file', 'quoted "name"', '中文目录']
child=Path('build/p5-p6-implementation-2026-10-10/argument_probe.py').resolve()
child.write_text('import sys,json\nfrom pathlib import Path\nPath(sys.argv[1]).write_text(json.dumps(sys.argv[2:]),encoding="utf-8")\n',encoding='utf-8')
args_file=output.resolve()/'arguments.json'
assert trace([sys.executable,str(child),str(args_file),*expected],output,Path('C:/Program Files/LLVM/bin/lldb.exe'),60,100000)==0
actual=json.loads(args_file.read_text(encoding='utf-8'))
assert actual==expected,(actual,expected)
print('Exact argument roundtrip passed')
