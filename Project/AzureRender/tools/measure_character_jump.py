"""Measure character motion using the production fixed-step physics fixture."""
import argparse,subprocess
from pathlib import Path
if __name__=='__main__':
    p=argparse.ArgumentParser(__doc__);p.add_argument('--executable',type=Path,required=True);p.add_argument('--output',type=Path,required=True)
    a=p.parse_args();a.output.parent.mkdir(parents=True,exist_ok=True)
    subprocess.run([str(a.executable.resolve()),'--measure',str(a.output.resolve())],check=True)
