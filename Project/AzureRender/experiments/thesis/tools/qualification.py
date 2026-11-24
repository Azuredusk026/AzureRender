"""Evaluate the color-only, single-sample thesis workload from actual queries."""
import argparse
import json
from pathlib import Path


def evaluate(device, width=2560, height=1440):
    queues = [q for q in device.get('queues', [])
              if q.get('graphics') and q.get('timestampValidBits', 0) > 0]
    formats = {f['name']: f for f in device.get('formats', [])}
    reasons = []
    if not queues:
        reasons.append('graphics queue with valid timestamps required')
    for name in ['albedo', 'normalDepth', 'depth', 'toon', 'output']:
        f = formats.get(name, {})
        if not (f.get('supported') and f.get('maxWidth', 0) >= width
                and f.get('maxHeight', 0) >= height and f.get('sampleCounts', 0) & 1):
            reasons.append(f'{name}: actual image usage/extent/sample combination unsupported')
    if device.get('maxColorAttachments', 0) < 3:
        reasons.append('three color attachment slots required')
    traditional = not reasons
    subpasses = traditional and device.get('maxPerStageDescriptorInputAttachments', 0) >= 2
    if not subpasses and traditional:
        reasons.append('two fragment input attachments required for local paths')
    local_api = device.get('apiVersion', 0) >= 4210688 or (
        'VK_KHR_dynamic_rendering_local_read' in device.get('extensions', []))
    drlr = bool(subpasses and local_api and device.get('dynamicRendering')
                and device.get('dynamicRenderingLocalRead'))
    return dict(traditional=traditional, subpasses=subpasses, drlr=drlr,
                queueFamily=queues[0]['index'] if queues else None,
                reasons=reasons, extent=[width, height],
                scope='capability-only; execution and visual qualification pending')


def main():
    parser = argparse.ArgumentParser(__doc__)
    parser.add_argument('input', type=Path)
    parser.add_argument('--output', type=Path, required=True)
    args = parser.parse_args()
    raw = json.loads(args.input.read_text(encoding='utf-8-sig'))
    result = dict(schemaVersion=1, devices=[dict(name=d['name'], **evaluate(d))
                                          for d in raw['devices']])
    args.output.write_text(json.dumps(result, indent=2) + '\n', encoding='utf-8')
    print(json.dumps(result, indent=2))


if __name__ == '__main__':
    main()
