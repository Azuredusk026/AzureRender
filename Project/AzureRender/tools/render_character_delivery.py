"""Render and verify the fixed 1440p character video delivery."""
import argparse
import hashlib
import json
from pathlib import Path
import re
import subprocess
import sys
import time
from encode_character_video import main as encode_main


def digest(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()


def inputs(executable, asset):
    source = Path(__file__).resolve().parents[1]
    sources = [p for directory in ('src', 'shaders') for p in (source / directory).rglob('*')
               if p.is_file() and p.suffix in ('.cpp', '.hpp', '.vert', '.frag', '.comp', '.glsl')]
    sources.append(source / 'CMakeLists.txt')
    return dict(assetSha256=digest(asset), executableSha256=digest(executable),
                shaderSha256={p.name: digest(p) for p in sorted((executable.parent / 'shaders').glob('*.spv'))},
                sourceSha256={p.relative_to(source).as_posix(): digest(p) for p in sorted(sources)})


def main():
    parser = argparse.ArgumentParser(__doc__)
    for name in ('executable', 'asset', 'output'):
        parser.add_argument('--' + name, type=Path, required=True)
    args = parser.parse_args()
    executable, asset, root = args.executable.resolve(), args.asset.resolve(), args.output.resolve()
    root.mkdir(parents=True, exist_ok=True)
    assert not any(root.iterdir()), 'Use an empty delivery directory'
    frozen = inputs(executable, asset)
    frames, timing = root / 'frames', root / 'gpu-timing.json'
    command = [str(executable), '--asset', str(asset), '--portfolio', '--anti-aliasing', 'supersample',
               '--width', '2560', '--height', '1440', '--capture-fps', '24', '--capture-frames', '384',
               '--fixed-frame-step', '--set', 'render.shadowDistance=10', '--gpu-timing',
               '--gpu-timing-output', str(timing), '--capture-dir', str(frames)]
    (root / 'capture-inputs.json').write_text(json.dumps(dict(**frozen, command=command), indent=2), encoding='utf-8')
    startup = None
    if hasattr(subprocess, 'STARTUPINFO'):
        startup = subprocess.STARTUPINFO()
        startup.dwFlags |= subprocess.STARTF_USESHOWWINDOW
        startup.wShowWindow = 0
    started = time.time()
    with (root / 'stdout.log').open('w', encoding='utf-8') as stdout, (root / 'stderr.log').open('w', encoding='utf-8') as stderr:
        result = subprocess.run(command, cwd=executable.parent, stdout=stdout, stderr=stderr,
                                startupinfo=startup, timeout=1800)
    elapsed = time.time() - started
    log = ''.join((root / name).read_text(encoding='utf-8') for name in ('stdout.log', 'stderr.log'))
    assert result.returncode == 0 and 'VUID-' not in log and 'Validation Error' not in log, log[-4000:]
    assert 'Allocator after unload: buffers=0 images=0' in log
    assert inputs(executable, asset) == frozen, 'Capture inputs changed during rendering'
    manifest = json.loads((frames / 'capture_manifest.json').read_text(encoding='utf-8'))
    for key, expected in dict(width=2560, height=1440, fps=24, capturedFrames=384,
                              antiAliasing=2, sceneRenderWidth=5120, sceneRenderHeight=2880).items():
        assert manifest[key] == expected, (key, manifest[key])
    assert len(list(frames.glob('frame_*.png'))) == 384
    gpu = json.loads(timing.read_text(encoding='utf-8'))
    performance = dict(renderWallSeconds=elapsed, launchToFirstPngSeconds=(frames / 'frame_000000.png').stat().st_mtime - started,
                       gpuAverageMs=gpu['totalAverageMs'], gpuP95Ms=gpu['totalP95Ms'],
                       deviceLocalPeakBytes=gpu['allocator']['deviceLocalPeakBytes'],
                       loadingMetric='Launch through asset loading, texture preparation, IBL, first render and PNG creation')
    marks = {}
    for row in log.splitlines():
        match = re.match(r'\[(\d+)\].*Character (texture access|IBL):', row)
        if match:
            marks[match[2]] = int(match[1])
    if len(marks) == 2:
        performance['texturePreparationThroughIblWallMs'] = marks['IBL'] - marks['texture access']
        performance['textureMetric'] = 'CPU mip generation, texture transfer/synchronization, shared texture setup and IBL preparation combined'
    (root / 'performance.json').write_text(json.dumps(performance, indent=2), encoding='utf-8')
    print('384 frames rendered; encoding and independently checking MP4', flush=True)
    video = root / 'laevatain-1440p24.mp4'
    saved = sys.argv
    try:
        sys.argv = [saved[0], '--frames', str(frames), '--output', str(video)]
        encode_main()
    finally:
        sys.argv = saved
    delivery = json.loads(video.with_suffix('.delivery.json').read_text(encoding='utf-8'))
    report = dict(schemaVersion=1, passed=True, **frozen, delivery=delivery, performance=performance,
                  captureManifestSha256=digest(frames / 'capture_manifest.json'),
                  frameSha256={p.name: digest(p) for p in sorted(frames.glob('frame_*.png'))})
    provenance = asset.with_suffix(asset.suffix + '.provenance.json')
    if provenance.is_file():
        report['assetProvenanceSha256'] = digest(provenance)
    (root / 'manifest.json').write_text(json.dumps(report, indent=2), encoding='utf-8')
    print(json.dumps(dict(video=str(video), passed=True, performance=performance)), flush=True)


if __name__ == '__main__':
    main()
