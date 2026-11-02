from pathlib import Path
run=Path(__file__).resolve().parent;text=(run/'capture_repeat.py').read_text();text=text.replace('import hashlib,json,os,re,subprocess,time','import argparse,hashlib,json,os,re,subprocess,time');text=text.replace("root=Path.cwd();run=Path(__file__).resolve().parent;out=run/'compatibility-repeat/full-world-hidden';", "parser=argparse.ArgumentParser();parser.add_argument('--name',choices=['serial-a','serial-b'],required=True);args=parser.parse_args();root=Path.cwd();run=Path(__file__).resolve().parent;out=run/'determinism-captures'/args.name;")
text=text.replace("'--runtime-report',str(out/'runtime.json')];env=", "'--runtime-report',str(out/'runtime.json'),'--disable-parallel-recording'];env=")
text=text.replace("(run/'compatibility-repeat.json')", "(run/('determinism-'+args.name+'.json'))")
assert "'--disable-parallel-recording'" in text and "'determinism-captures'" in text
(run/'capture_determinism.py').write_bytes(text.encode());print('Prepared one-variable serial recording controls')
