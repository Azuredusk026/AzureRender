"""Integration check: real Vulkan execution, timing and nonempty image."""
import json
from pathlib import Path
import subprocess
import sys

exe, shaders, output, path = sys.argv[1:]
root = Path(output)
root.mkdir(parents=True, exist_ok=True)
assert Path(exe).is_file(), 'real benchmark executable must exist'
result = subprocess.run([exe, '--shaders', shaders, '--output', output, '--path', path,
                         '--gpu', 'NVIDIA', '--width', '128', '--height', '128',
                         '--frames', '8', '--warmup', '2', '--validation'], timeout=60)
assert result.returncode == 0, 'real benchmark must execute successfully'
data = json.loads((root / 'run.json').read_text())
assert data['path'] == path
assert data['validationEnabled'] and data['validationErrors'] == 0, data
assert len(data['gpuMs']) == len(data['cpuRecordingMs']) == 8
assert all(x > 0 for x in data['gpuMs']), data
assert data['geometryDraws'] == data['toonDraws'] == data['contourDraws'] == 10
assert data['nextSubpassCalls'] == (10 if path == 'subpasses' else 0)
assert data['beginRenderingCalls'] == (10 if path == 'drlr' else 0)
rgba = (root / 'image.rgba').read_bytes()
assert len(rgba) == 128*128*4
pixels={tuple(rgba[i:i+4]) for i in range(0,len(rgba),4)}
assert len(pixels) == 5, 'background, three toon bands and contour must all be visible'
assert all(rgba[i] == 255 for i in range(3,len(rgba),4)), 'opaque output required'
print(path, 'execution, eight timestamps, three stages, validation and image passed')
