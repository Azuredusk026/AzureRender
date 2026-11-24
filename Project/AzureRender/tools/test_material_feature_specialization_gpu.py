"""Verify common material-feature policies through independent Player fixtures."""
import argparse
import json
from pathlib import Path
import re
import subprocess

import test_screen_space_reflections_gpu as fixture


def run(player, output, aa, reference=None):
    player = player.resolve()
    output = output.resolve()
    output.mkdir(parents=True, exist_ok=False)
    project, _ = fixture.prepare(output, dict(name=f'feature-aa{aa}', enabled=False, lit=True))
    for name, features in [('floor', ['neutral-fallback']),
                           ('target', ['neutral-fallback', 'emissive-mask'])]:
        path = output / 'assets' / (name + '.gltf')
        document = json.loads(path.read_text(encoding='utf-8'))
        document['materials'][0]['extras'] = dict(azureRenderMaterial=dict(
            schemaVersion=1, **{'class': 'fabric'}, features=features))
        fixture.write_json(path, document)
    level = output / 'assets/scene.azurelevel'
    document = json.loads(level.read_text(encoding='utf-8'))
    document['renderSettings'].update(schemaVersion=20, antiAliasing=aa)
    fixture.write_json(level, document)
    frames = 6 if aa == 3 else 2
    command = [str(player), '--project', str(project), '--width', '192', '--height', '128',
               '--capture-dir', str(output / 'frames'), '--capture-frames', str(frames),
               '--fixed-frame-step', '--qa-camera', 'scene', '--qa-light', 'scene',
               '--qa-isolation', 'beauty', '--runtime-report', str(output / 'runtime.json')]
    fixture.write_json(output / 'command.json', command)
    inputs = {p.relative_to(output).as_posix(): fixture.sha(p)
              for p in (output / 'assets').rglob('*') if p.is_file()}
    shaders = {p.name: fixture.sha(p) for p in (player.parent / 'shaders').glob('*.spv')}
    report = dict(schemaVersion=1, status='running', antiAliasing=aa,
                  playerSha256=fixture.sha(player), inputs=inputs, shaders=shaders)
    try:
        startup = None
        if hasattr(subprocess, 'STARTUPINFO'):
            startup = subprocess.STARTUPINFO()
            startup.dwFlags |= subprocess.STARTF_USESHOWWINDOW
            startup.wShowWindow = 0
        with (output / 'player.log').open('w', encoding='utf-8') as log:
            subprocess.run(command, stdout=log, stderr=subprocess.STDOUT,
                           startupinfo=startup, check=True, timeout=180)
        log = (output / 'player.log').read_text(encoding='utf-8', errors='replace')
        assert not re.search(r'VUID-|Validation (?:Error|Warning)|\[error\]', log)
        assert 'Allocator after unload: buffers=0 images=0' in log
        images = sorted((output / 'frames').glob('frame_*.png'))
        assert len(images) == frames
        report['images'] = {p.name: fixture.sha(p) for p in images}
        manifest = json.loads((output / 'frames/capture_manifest.json').read_text(encoding='utf-8-sig'))
        report['manifest'] = manifest
        assert manifest['antiAliasing'] == aa
        assert 0 < manifest['materialClassPipelineCount'] <= 40
        policies = manifest.get('materialClassFeatureSpecializations')
        assert policies is not None, 'Player capture lacks material-class feature specialization policies'
        policy = next(p for p in policies if p['class'] == 4)
        # 32 is shared; bit 3 varies; the two view-owned bits stay dynamic.
        assert policy['fixedMask'] == 0x3ffffff7, policy
        assert policy['fixedValue'] == 32, policy
        if reference:
            previous = json.loads((reference / 'result.json').read_text(encoding='utf-8'))
            assert previous['inputs'] == inputs
            assert previous['images'] == report['images'], 'Feature specialization changed frozen fixture PNGs'
            report['referencePngPairsExact'] = frames
        assert fixture.sha(player) == report['playerSha256']
        assert shaders == {p.name: fixture.sha(p) for p in (player.parent / 'shaders').glob('*.spv')}
        assert all(fixture.sha(output / name) == digest for name, digest in inputs.items())
        report['status'] = 'passed'
    except Exception as error:
        report.update(status='failed', error=str(error))
        raise
    finally:
        fixture.write_json(output / 'result.json', report)
    print(json.dumps(dict(status=report['status'], antiAliasing=aa,
                          policies=manifest['materialClassFeatureSpecializations'])))


if __name__ == '__main__':
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--player', type=Path, required=True)
    parser.add_argument('--output', type=Path)
    parser.add_argument('--aa', type=int, choices=[0, 3, 4], default=0)
    parser.add_argument('--reference', type=Path)
    args = parser.parse_args()
    if args.output:
        run(args.player, args.output, args.aa, args.reference)
    else:
        import tempfile
        runs = Path(__file__).resolve().parents[1] / 'build/runs'
        runs.mkdir(parents=True, exist_ok=True)
        with tempfile.TemporaryDirectory(prefix='material-feature-',
                                         dir=runs) as folder:
            run(args.player, Path(folder) / 'capture', args.aa, args.reference)
