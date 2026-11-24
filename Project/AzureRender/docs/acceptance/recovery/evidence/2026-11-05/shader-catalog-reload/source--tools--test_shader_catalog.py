"""Compile the generated catalog within its production deadline and compare every binary."""
from pathlib import Path
import argparse
import hashlib
import json
import subprocess
import sys


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--executable', type=Path, required=True)
    parser.add_argument('--config', type=Path, required=True)
    args = parser.parse_args()
    engine = Path(__file__).resolve().parents[1]
    run = Path(subprocess.check_output([
        sys.executable, str(engine / 'tools/artifact_cleanup.py'), '--new-run', 'shader-catalog'
    ], cwd=engine, text=True).strip())
    command = [str(args.executable.resolve()), '--compile-catalog',
               str(args.config.resolve()), str(run / 'work')]
    with (run / 'compiler.log').open('wb') as log:
        result = subprocess.run(command, cwd=engine, stdout=log, stderr=log, timeout=90)
    report = json.loads((run / 'work/result.json').read_bytes())
    report.update(command=command, exitCode=result.returncode,
                  executableSha256=hashlib.sha256(args.executable.read_bytes()).hexdigest(),
                  configSha256=hashlib.sha256(args.config.read_bytes()).hexdigest())
    (run / 'summary.json').write_text(json.dumps(report, indent=2) + '\n', encoding='utf-8')
    print(json.dumps(dict(run=str(run), **report)), flush=True)
    assert result.returncode == 0 and report['status'] == 'passed', report
    subprocess.run([sys.executable, str(engine / 'tools/artifact_cleanup.py'),
                    '--finish', str(run)], cwd=engine, check=True)


if __name__ == '__main__':
    main()
