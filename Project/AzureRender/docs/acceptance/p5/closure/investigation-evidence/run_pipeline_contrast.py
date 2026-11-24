from pathlib import Path
import subprocess,json,sys,hashlib
root=Path.cwd();work=root/'build/p5-p6-closure-2026-10-10';mode=sys.argv[1]
out=work/('pipeline-'+mode);out.mkdir(exist_ok=False)
disabled=mode=='unoptimized';module=work/'pipeline_flags_probe.py';config=out/'config.json'
config.write_text(json.dumps(dict(disableOptimization=disabled,output=str(out/'probe.json'),
    moduleHash=hashlib.sha256(module.read_bytes()).hexdigest(),diagnosticOnly=True)))
def quote(v):return '"'+str(v).replace('\\','\\\\').replace('"','\\"')+'"'
args=['--project',str(work/'formal-source-verified/exploration/project.azureproject'),
    '--width','1920','--height','1080','--fixed-frame-step','--smoke-frames','2100',
    '--game-actions',str(work/'formal-source-verified/exploration-1.input.json'),
    '--gpu-timing','--gpu-timing-output',str(out/'timing.json')]
commands=out/'commands.lldb'
commands.write_text('\n'.join(['target create '+quote(root/'build/ninja-msvc-release-symbols/AzurePlayer.exe'),
    'command script import '+quote(module),
    'process launch -s -o '+quote(out/'stdout.log')+' -e '+quote(out/'stderr.log')+' -- '+' '.join(quote(a) for a in args),
    'script pipeline_flags_probe.configure(lldb.debugger, '+repr(str(config))+')',
    'continue','script pipeline_flags_probe.finish(lldb.debugger)','quit','']))
# Reuse the owned-child observer without changing production source.
observer=(work/'trace_exploration.py').read_text()
begin=observer.index('command=[sys.executable');end=observer.index('kernel=ctypes.WinDLL')
observer=observer[:begin]+"command=['C:/Program Files/LLVM/bin/lldb.exe','-b','-s',"+repr(str(commands))+"]\n"+observer[end:]
# The tracer wrapper owns LLDB; this direct invocation owns LLDB itself.
observer=observer.replace("lldbs={pid for pid,parent,name in table if parent==process.pid and name.lower()=='lldb.exe'}","lldbs={process.pid}")
observer=observer.replace("out/'exploration-trace-run.log'",repr(str(out/'run.log')))
observer=observer.replace("out/'exploration-trace-residency.json'",repr(str(out/'residency.json')))
observer=observer.replace("with ("+repr(str(out/'run.log'))+").open", "with Path("+repr(str(out/'run.log'))+").open")
observer=observer.replace("("+repr(str(out/'residency.json'))+").write_text","Path("+repr(str(out/'residency.json'))+").write_text")
exec(compile(observer,str(work/'trace_exploration.py'),'exec'))
