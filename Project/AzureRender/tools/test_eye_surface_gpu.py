"""Check public radial eye surfaces through immutable, independent GPU fixtures.

This suite imports no production eye mathematics. The thirteen prepared pixel
oracles are reused verbatim. Extra cases use constants, linearity, and isolated
positive/negative controls. Native NPR equivalence is outside this contract.
"""
import argparse
import base64
import copy
import hashlib
from io import BytesIO
import json
from pathlib import Path
import re
import shutil
import subprocess

import numpy as np
from PIL import Image


SIZE = 128
BUDGET = .008
FEATURE = 'eye-radial-surface'


def sha(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()


def save(path, value):
    path.write_text(json.dumps(value, ensure_ascii=False, indent=2) + '\n', encoding='utf8')


def linear(value):
    value = np.asarray(value, dtype=np.float64) / 255
    return np.where(value <= .04045, value / 12.92, ((value + .055) / 1.055) ** 2.4)


def rgba_uri(value):
    stream = BytesIO()
    Image.new('RGBA', (4, 1), tuple(value)).save(stream, format='PNG')
    return 'data:image/png;base64,' + base64.b64encode(stream.getvalue()).decode()


def read(path):
    return json.loads(path.read_text(encoding='utf-8-sig'))


def fixtures_at(root):
    index = read(root / 'index.json')
    cases = {}
    for item in index['cases']:
        name = item['name']
        folder = root / name
        expected = read(folder / 'expected.json')
        assert expected['nativeEquivalenceClaimed'] is False, name
        assert expected['maximumLinearError'] == BUDGET, name
        model = read(folder / 'assets/eye.gltf')
        profile = model['materials'][0]['extras']['azureRenderMaterial']
        assert profile['class'] == 'eye' and profile['features'] == [FEATURE], name
        assert profile['eyeSurface']['lightingInputs']['source'] == 'host-authored', name
        samples = expected['samples']
        wanted = np.asarray([sample['linearRgb'] for sample in samples], dtype=float)
        assert wanted.shape == (3, 3) and np.isfinite(wanted).all(), name
        assert ((wanted >= 0) & (wanted <= 1)).all(), name
        cases[name] = {'name': name, 'source': name, 'expected': wanted,
                       'pixels': [sample['pixel'] for sample in samples], 'edit': None}
    assert len(cases) == 13, 'Expected the thirteen prepared eye oracles'
    return cases


def definitions(prepared):
    result = list(prepared.values())

    def add(name, original, expected, edit=None, fixed=False):
        item = copy.copy(prepared[original])
        item.update(name=name, source=original, expected=expected, edit=edit, fixed=fixed)
        result.append(item)

    mc = prepared['matcap-rgba']['expected']
    direct = prepared['direct-parallax']['expected']
    constant = prepared['direct-linear-control']['expected']
    for original in ('direct-parallax', 'matcap-rgba'):
        add(original + '-fixed', original, prepared[original]['expected'], fixed=True)
    add('matcap-dark', 'matcap-rgba', np.zeros_like(mc), 'dark')
    add('matcap-half-light', 'matcap-rgba', mc * .5, 'half-light')
    add('matcap-colored-light', 'matcap-rgba', mc * [.25, .5, .75], 'colored-light')
    add('matcap-base-color-independent', 'matcap-rgba', mc, 'matcap-base-control')
    add('direct-light-isolated', 'direct-parallax', direct, 'direct-isolated')
    add('direct-alpha-premultiply', 'direct-linear-control', constant * (128 / 255), 'premultiply')
    diffuse = np.tile(linear([128, 64, 224]) * (np.array([32, 64, 128]) / 255) * .4, (3, 1))
    for suffix, factor, edit, fixed in (
            ('light', 1, 'diffuse', False), ('dark', 0, 'diffuse-dark', False),
            ('half-light', .5, 'diffuse-half', False),
            ('colored-light', np.array([.25, .5, .75]), 'diffuse-colored', False),
            ('premultiply', 128 / 255, 'diffuse-premultiply', False),
            ('fixed', 1, 'diffuse', True)):
        add('host-diffuse-' + suffix, 'direct-linear-control', diffuse * factor, edit, fixed)
    add('highlight-outside', 'direct-circle-border', np.tile([.12, .28, .44], (3, 1)), 'highlight')
    add('highlight-inside', 'direct-no-parallax', np.zeros((3, 3)), 'highlight')
    add('scattering-direct', 'direct-linear-control',
        np.tile(np.array([.2, .4, .6]) * (128 / 255), (3, 1)), 'scattering')
    # A red primary owns shadows; the green secondary never owns a shadow map.
    # Both have N.L=.8. White Matcap and view-ramp alpha=1 give green=.4*.8.
    # The secondary light-ramp alpha=0 must not replace that view-ramp value.
    for state, red in (('clear', 1), ('shadowed', 0)):
        add('matcap-secondary-directional-' + state, 'matcap-rgba',
            np.tile([red, .32, 0], (3, 1)), 'secondary-directional-' + state)
    for category in ('generic', 'eye'):
        add('legacy-' + category + '-dark', 'direct-linear-control', None, 'legacy-' + category + '-dark')
        add('legacy-' + category + '-light', 'direct-linear-control', None, 'legacy-' + category + '-light')
    return result


def edit_fixture(model, level, mode):
    if mode is None:
        return
    material = model['materials'][0]
    profile = material['extras']['azureRenderMaterial']
    eye = profile['eyeSurface']
    light = level['lights'][0]
    if mode == 'dark': light['enabled'] = False
    elif mode == 'half-light': light['intensity'] = .2
    elif mode == 'colored-light': light['color'] = [.25, .5, .75]
    elif mode == 'matcap-base-control':
        # Zero RGB ramp excludes host diffuse; retained ramp alpha drives Matcap.
        model['images'][0]['uri'] = rgba_uri((16, 128, 224, 0))
        model['images'][1]['uri'] = rgba_uri((0, 0, 0, 128))
    elif mode == 'direct-isolated':
        light['enabled'] = True
        model['images'][1]['uri'] = rgba_uri((0, 0, 0, 128))
        eye['matcapColor'] = [0, 0, 0, 0]
    elif mode == 'premultiply': eye['alphaPremultiply'] = 1
    elif mode.startswith('diffuse'):
        eye.update(parallaxScale=0, uvScaleOffset=[0, 0, .5, .5],
                   highlightColor=[1, 1, 1], scatteringColor=[1, 1, 1], matcapColor=[0, 0, 0, 0])
        eye['lightingInputs']['directWeights'] = [0, 0, 0]
        if mode == 'diffuse-premultiply': eye['alphaPremultiply'] = 1
        model['images'][1]['uri'] = rgba_uri((32, 64, 128, 128))
        light.update(enabled=mode != 'diffuse-dark', intensity=.2 if mode == 'diffuse-half' else .4,
                     color=[.25, .5, .75] if mode == 'diffuse-colored' else [1, 1, 1])
    elif mode == 'highlight':
        light['enabled'] = False
        eye.update(highlightColor=[.12, .28, .44], scatteringColor=[0, 0, 0])
        eye['lightingInputs']['directWeights'] = [0, 1, 0]
    elif mode == 'scattering':
        light['enabled'] = False
        eye['scatteringColor'] = [.2, .4, .6]
        eye['lightingInputs']['directWeights'] = [0, 0, 1]
    elif mode.startswith('secondary-directional-'):
        eye.update(parallaxScale=0, matcapNormalScale=0, matcapColor=[0, 0, 0, 1])
        eye['lightingInputs']['directWeights'] = [0, 0, 0]
        model['images'][0]['uri'] = rgba_uri((0, 0, 0, 0))
        # Only the last texel has alpha=1. View U=1 samples it; light U=.9
        # samples alpha=0. The independent target needs no eye-math helper.
        ramp = Image.new('RGBA', (16, 1), (0, 0, 0, 0))
        ramp.putpixel((15, 0), (0, 0, 0, 255))
        stream = BytesIO()
        ramp.save(stream, format='PNG')
        model['images'][1]['uri'] = 'data:image/png;base64,' + base64.b64encode(stream.getvalue()).decode()
        model['images'][2]['uri'] = rgba_uri((255, 255, 255, 0))
        light.update(color=[1, 0, 0], intensity=3)
        light['shape']['direction'] = [.6, 0, -.8]
        secondary = copy.deepcopy(light)
        secondary.update(id='secondary', nodeId='secondary', color=[0, 1, 0], intensity=.4)
        secondary['shape']['direction'] = [-.6, 0, -.8]
        level['lights'].append(secondary)
        level['nodes'].append({'id': 'secondary', 'components': {'azure.transform': {
            'type': 'azure.transform', 'version': 1, 'data': {}}}})
        level['renderSettings'].update(shadowRadius=1, shadowDistance=20)
        if mode.endswith('-shadowed'):
            # Emitted direction (.6,0,-.8) projects (-1.5,0,2) onto the eye.
            # This opaque quad stays outside the camera frustum and does not
            # lie along the green secondary's ray to the sampled receiver.
            level['nodes'].append({'id': 'blocker', 'resourceId': 'eye', 'components': {
                'azure.transform': {'type': 'azure.transform', 'version': 1, 'data': {
                    'translation': [-1.5, 0, 2], 'scale': [.8, .8, 1]}}}})
    elif mode.startswith('legacy-'):
        _, category, state = mode.split('-')
        material['extras']['azureRenderMaterial'] = {
            'schemaVersion': 2, 'class': category, 'features': []}
        light['enabled'] = state == 'light'
    else:
        raise ValueError('Unknown fixture edit: ' + mode)


def oracle_preflight(cases):
    wanted = {case['name']: case['expected'] for case in cases}
    for a, b in (('direct-parallax', 'direct-no-parallax'),
                 ('matcap-ramp-alpha-low', 'matcap-ramp-alpha-high'),
                 ('matcap-rgba', 'matcap-mirror'), ('matcap-rgba', 'matcap-rotated')):
        assert float(np.abs(wanted[a] - wanted[b]).max()) > 2 * BUDGET, (a, b)
    assert np.allclose(wanted['matcap-rgba'], wanted['matcap-alpha-only'] + wanted['matcap-rgb-only'])
    assert float(wanted['all-zero'].max()) == 0
    assert float(wanted['host-diffuse-light'].max()) > 2 * BUDGET


def fingerprints(player, fixture_root):
    paths = [Path(__file__).resolve()]
    paths += sorted(path for path in fixture_root.rglob('*') if path.is_file() and '__pycache__' not in path.parts)
    for name in ('contract.json', 'source-math-audit.json', 'eye_math_oracle.py', 'prepare_eye_candidate.py'):
        path = fixture_root.parent / name
        if path.exists(): paths.append(path)
    if player is not None:
        paths.append(player.resolve())
        shaders = sorted((player.parent / 'shaders').glob('*.spv'))
        assert shaders, 'Player shader directory must contain compiled SPIR-V'
        paths += shaders
    return {str(path.resolve()): sha(path) for path in paths}


def run(player, fixture_root, output, expected_validation, selected):
    assert not output.is_relative_to(fixture_root), 'Capture output must stay outside immutable source fixtures'
    all_cases = definitions(fixtures_at(fixture_root))
    oracle_preflight(all_cases)
    cases = [case for case in all_cases if not selected or case['name'] in selected]
    assert cases and (not selected or set(selected) <= {case['name'] for case in all_cases}), 'Unknown case filter'
    output.mkdir(parents=True, exist_ok=False)
    frozen = fingerprints(player, fixture_root)
    save(output / 'input-fingerprints.json', frozen)
    summary = {'schemaVersion': 1, 'status': 'running', 'feature': FEATURE, 'featureBit': 17,
               'linearErrorBudget': BUDGET, 'nativeEquivalenceClaimed': False,
               'scope': 'Public host-authored eye surface, independent fixtures and resource lifetime',
               'selectedCases': [case['name'] for case in cases], 'completeSuite': not selected,
               'captures': [], 'metrics': {}, 'failures': [], 'inputFingerprints': frozen,
               'limitations': ['Full native NPR illumination and original runtime uniforms remain uncaptured.',
                               'This suite does not validate original character art or native visual equivalence.']}
    measured = {}
    frames = {}
    try:
        for case in cases:
            name = case['name']
            print(json.dumps({'capture': name, 'status': 'starting'}), flush=True)
            folder = output / name
            shutil.copytree(fixture_root / case['source'], folder)
            asset_path, level_path = folder / 'assets/eye.gltf', folder / 'assets/scene.azurelevel'
            model, level = read(asset_path), read(level_path)
            edit_fixture(model, level, case['edit'])
            save(asset_path, model)
            save(level_path, level)
            # Save our independent target beside the exact generated inputs.
            save(folder / 'gpu-target.json', {'pixels': case['pixels'],
                 'linearRgb': None if case['expected'] is None else case['expected'].tolist(),
                 'oracleSource': case['source'], 'edit': case['edit'], 'maximumLinearError': BUDGET})
            command = [str(player.resolve()), '--project', str((folder / 'test.azureproject').resolve()),
                       '--width', str(SIZE), '--height', str(SIZE), '--capture-dir', str((folder / 'frames').resolve()),
                       '--capture-frames', '1', '--fixed-frame-step', '--qa-camera', 'scene', '--qa-light', 'scene',
                       '--qa-isolation', 'beauty', '--runtime-report', str((folder / 'runtime.json').resolve())]
            if case.get('fixed'):
                command += ['--disable-bindless', '--disable-compute-skinning', '--disable-parallel-recording']
            save(folder / 'command.json', command)
            result = subprocess.run(command, capture_output=True, encoding='utf8', errors='replace', timeout=120)
            log = result.stdout + result.stderr
            (folder / 'player.log').write_text(log, encoding='utf8')
            assert result.returncode == 0, (name, 'Player failed', result.returncode)
            assert not re.search(r'VUID-|Validation (?:Error|Warning)|\[error\]', log), (name, 'validation or runtime error')
            validation = 'Validation layer: enabled' in log
            if expected_validation != 'any':
                assert validation == (expected_validation == 'enabled'), (name, 'validation configuration')
            assert 'Allocator after unload: buffers=0 images=0' in log, (name, 'resource leak or missing unload')
            path_mode = 'fixed' if case.get('fixed') else 'indexed'
            assert 'Character texture access: ' + path_mode in log, (name, 'descriptor path not exercised', path_mode)
            runtime = read(folder / 'runtime.json')
            assert not runtime['scriptErrors'] and not runtime['presentationErrors'], (name, 'runtime report')
            manifest = read(folder / 'frames/capture_manifest.json')
            assert manifest['qaIsolation'] == 'beauty' and manifest['qaCamera'] == 'scene', name
            assert np.allclose(manifest['qaCameraPosition'], [0, 0, 3], atol=1e-6), (name, 'camera changed')
            frame = folder / 'frames/frame_000000.png'
            pixels = np.asarray(Image.open(frame).convert('RGB'))
            assert pixels.shape == (SIZE, SIZE, 3), (name, 'capture size')
            frames[name] = pixels.astype(np.int16)
            actual = np.asarray([linear(pixels[y, x]) for x, y in case['pixels']])
            measured[name] = actual
            error = None if case['expected'] is None else float(np.abs(actual - case['expected']).max())
            if error is not None and error > BUDGET:
                summary['failures'].append(name + ': linear error ' + str(error))
            if case['expected'] is not None and not np.any(case['expected']) and actual.max() != 0:
                summary['failures'].append(name + ': zero-energy control contains light')
            summary['captures'].append({'name': name, 'descriptorPath': path_mode, 'validationEnabled': validation,
                'maximumLinearError': error, 'linearRgb': actual.tolist(), 'imageSha256': sha(frame),
                'modelSha256': sha(asset_path), 'levelSha256': sha(level_path),
                'allocatorAfterUnload': {'buffers': 0, 'images': 0}})
        for source in ('direct-parallax', 'matcap-rgba', 'host-diffuse-light'):
            fixed = 'host-diffuse-fixed' if source == 'host-diffuse-light' else source + '-fixed'
            if source in frames and fixed in frames:
                difference = int(np.abs(frames[source] - frames[fixed]).max())
                summary['metrics'][source + 'FixedMaximumDifference'] = difference
                if difference: summary['failures'].append(source + ': fixed and indexed frames differ')
        for a, b in (('direct-parallax', 'direct-no-parallax'),
                     ('matcap-ramp-alpha-low', 'matcap-ramp-alpha-high'),
                     ('matcap-rgba', 'matcap-mirror'), ('matcap-rgba', 'matcap-rotated')):
            if a in measured and b in measured:
                delta = float(np.abs(measured[a] - measured[b]).max())
                summary['metrics'][a + ' vs ' + b] = delta
                if delta <= BUDGET: summary['failures'].append(a + '/' + b + ': discrimination lost')
        for category in ('generic', 'eye'):
            dark, light = 'legacy-' + category + '-dark', 'legacy-' + category + '-light'
            if dark in measured and light in measured:
                delta = float(np.abs(measured[light] - measured[dark]).max())
                summary['metrics'][category + 'LegacyLightingResponse'] = delta
                if delta <= .001: summary['failures'].append(category + ': legacy lighting control missing')
        if all(name in measured for name in ('matcap-rgba', 'matcap-alpha-only', 'matcap-rgb-only')):
            residual = float(np.abs(measured['matcap-rgba'] - measured['matcap-alpha-only'] - measured['matcap-rgb-only']).max())
            summary['metrics']['matcapRgbaAdditiveResidual'] = residual
            if residual > 2 * BUDGET: summary['failures'].append('Matcap RGB/alpha additivity lost')
        clear, blocked = 'matcap-secondary-directional-clear', 'matcap-secondary-directional-shadowed'
        if clear in measured and blocked in measured:
            green_delta = float(np.abs(measured[clear][:, 1] - measured[blocked][:, 1]).max())
            red_drop = float((measured[clear][:, 0] - measured[blocked][:, 0]).min())
            summary['metrics']['secondaryDirectionalMatcapShadowDelta'] = green_delta
            summary['metrics']['primaryDirectionalShadowRedDrop'] = red_drop
            if green_delta > BUDGET:
                summary['failures'].append('Secondary directional Matcap consumes another light shadow')
            if red_drop <= .05:
                summary['failures'].append('Primary directional shadow control is not active')
        assert frozen == fingerprints(player, fixture_root), 'Capture inputs changed during validation'
        assert not summary['failures'], summary['failures']
        summary['status'] = 'passed'
    except Exception as error:
        summary.update(status='failed', failure=str(error))
        raise
    finally:
        save(output / 'summary.json', summary)
    print(json.dumps({'status': summary['status'], 'caseCount': len(summary['captures']), 'completeSuite': not selected}))


if __name__ == '__main__':
    parser = argparse.ArgumentParser(__doc__)
    parser.add_argument('--executable', type=Path)
    parser.add_argument('--fixtures', type=Path, required=True)
    parser.add_argument('--output', type=Path)
    parser.add_argument('--expected-validation', choices=('any', 'enabled', 'disabled'), default='any')
    parser.add_argument('--case', action='append', default=[])
    parser.add_argument('--check-oracle', action='store_true', help='Read-only CPU preflight; does not start Player')
    args = parser.parse_args()
    fixture_root = args.fixtures.resolve()
    if args.check_oracle:
        prepared = definitions(fixtures_at(fixture_root))
        oracle_preflight(prepared)
        print(json.dumps({'status': 'oracle-preflight-passed', 'caseCount': len(prepared), 'gpuExecuted': False}))
    else:
        if args.executable is None:
            parser.error('--executable is required for GPU execution')
        if args.output is not None:
            run(args.executable.resolve(), fixture_root, args.output.resolve(), args.expected_validation, args.case)
        else:
            import tempfile
            with tempfile.TemporaryDirectory(prefix='eye-surface-', dir=Path(__file__).resolve().parents[1] / 'build/runs') as folder:
                run(args.executable.resolve(), fixture_root, Path(folder) / 'capture', args.expected_validation, args.case)
