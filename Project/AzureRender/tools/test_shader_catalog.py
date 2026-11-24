"""Compile the generated catalog within its production deadline and compare every binary."""
from pathlib import Path
import argparse
import hashlib
import json
import subprocess
import sys

from artifact_maintenance import digest, real_path


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--executable', type=Path, required=True)
    parser.add_argument('--config', type=Path, required=True)
    args = parser.parse_args()
    engine = Path(__file__).resolve().parents[1]
    config = args.config.resolve()
    descriptor = json.loads(config.read_bytes())
    binaries = real_path(config.parent / descriptor['binaryRoot'])
    copied_bytes = sum(p.stat().st_size for p in binaries.rglob('*.spv') if p.is_file())
    planned_bytes = copied_bytes + 64 * 1024 * 1024
    run = Path(subprocess.check_output([
        sys.executable, str(engine / 'tools/artifact_cleanup.py'), '--new-run', 'shader-catalog',
        '--planned-bytes', str(planned_bytes)
    ], cwd=engine, text=True).strip())
    command = [str(args.executable.resolve()), '--compile-catalog',
               str(config), str(run / 'work')]
    with (run / 'compiler.log').open('wb') as log:
        result = subprocess.run(command, cwd=engine, stdout=log, stderr=log, timeout=90)
    report = json.loads((run / 'work/result.json').read_bytes())
    report.update(command=command, exitCode=result.returncode,
                  executableSha256=hashlib.sha256(args.executable.read_bytes()).hexdigest(),
                  configSha256=hashlib.sha256(config.read_bytes()).hexdigest(),
                  plannedBytes=planned_bytes)
    (run / 'summary.json').write_text(json.dumps(report, indent=2) + '\n', encoding='utf-8')
    print(json.dumps(dict(run=str(run), **report)), flush=True)
    assert result.returncode == 0 and report['status'] == 'passed', report
    assert report['byteExactPrograms'] == report['programs'] == len(descriptor['programs'])
    candidate = real_path(run / 'work/1/binary')
    assert real_path(run) in candidate.parents
    copies = {}
    for path in sorted(candidate.rglob('*.spv')):
        relative = path.relative_to(candidate)
        original = real_path(binaries / relative)
        assert binaries in original.parents and real_path(path) == path
        sha256 = digest(path)
        assert sha256 == digest(original), relative
        copies[relative.as_posix()] = dict(bytes=path.stat().st_size, sha256=sha256)
    assert {entry['output'] for entry in descriptor['programs']} <= copies.keys()
    (run / 'binary-copy-sha256.json').write_text(json.dumps(copies, indent=2) + '\n', encoding='utf-8')
    for name in copies:
        (candidate / name).unlink()
    report.update(reclaimedBinaryCopies=len(copies), reclaimedBytes=sum(v['bytes'] for v in copies.values()))
    (run / 'summary.json').write_text(json.dumps(report, indent=2) + '\n', encoding='utf-8')
    subprocess.run([sys.executable, str(engine / 'tools/artifact_cleanup.py'),
                    '--finish', str(run)], cwd=engine, check=True)


if __name__ == '__main__':
    main()
