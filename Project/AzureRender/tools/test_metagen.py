import pathlib,subprocess,sys,tempfile,time
exe=pathlib.Path(sys.argv[1]).resolve()
with tempfile.TemporaryDirectory() as directory:
 root=pathlib.Path(directory);source=root/'components.hpp';out=root/'generated.hpp'
 source.write_text('namespace demo {\nAZURE_TYPE("test.body", 1)\nstruct Body {\n AZURE_FIELD("Speed", 0, 20)\n float speed = 5;\n};\n}\n')
 def run():return subprocess.run([str(exe),str(out),str(source)],capture_output=True,text=True)
 result=run();assert result.returncode==0,result.stderr
 first=out.read_bytes();stamp=out.stat().st_mtime_ns;time.sleep(0.02)
 assert run().returncode==0 and out.stat().st_mtime_ns==stamp,'Unchanged generation rewrote the output'
 source.write_text(source.read_text().replace('float speed','double speed'))
 result=run();assert result.returncode!=0 and ':4:' in result.stderr,result.stderr
 assert out.read_bytes()==first,'Failed generation damaged the output'
 source.write_text('namespace demo {\nAZURE_TYPE("test.body", 1)\nstruct Body {\n AZURE_FIELD("Speed", 0, 20)\n float speed = 5;\n};\nAZURE_TYPE("test.body", 1)\nstruct Other {};\n}\n')
 assert run().returncode!=0,'Duplicate stable type was accepted'
 print('Incremental generation, source diagnostics and failed-output retention passed')
