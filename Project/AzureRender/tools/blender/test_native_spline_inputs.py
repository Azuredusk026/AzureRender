"""Exercise spline conversion with proper rigid and non-rigid world inputs."""
import argparse
import copy
import json
from pathlib import Path
import sys

from mathutils import Euler, Matrix

parser = argparse.ArgumentParser()
parser.add_argument('--build-command', type=Path, required=True)
parser.add_argument('--output', type=Path, required=True)
args = parser.parse_args(sys.argv[sys.argv.index('--') + 1:])
args.output = args.output.resolve()
args.output.mkdir(parents=True, exist_ok=False)
command = json.loads(args.build_command.read_text(encoding='utf-8-sig'))
original = command[command.index('--') + 1:]
sys.path.insert(0, str(Path(__file__).parent))
import build_native_spline_clips as builder


def value(arguments, name):
    return arguments[arguments.index('--' + name) + 1]


inventory = json.loads(Path(value(original, 'inventory')).read_text(encoding='utf-8-sig'))
emitter = next(e for e in inventory['emitters'] if e['identity'][0] == value(original, 'source'))
inventory['emitters'] = [emitter]
inventory_path = args.output / 'inventory.json'
inventory_path.write_text(json.dumps(inventory), encoding='utf-8')
worlds = json.loads(Path(value(original, 'world')).read_text(encoding='utf-8-sig'))
source = next(row for row in worlds if row['identity'] == emitter['identity'])
base = Matrix(source['candidate_world_matrix_row_major'])
scaled = base @ Matrix.Diagonal((2., 1., 1., 1.))
sheared = base @ Matrix(((1., .2, 0., 0.), (0., 1., 0., 0.),
                        (0., 0., 1., 0.), (0., 0., 0., 1.)))
reflected = base @ Matrix.Diagonal((-1., 1., 1., 1.))
non_affine = base.copy()
non_affine[3][0] = .2
rotated = Euler((.3, -.4, .8), 'XYZ').to_matrix().to_4x4()
rotated.translation = (13., -7., 2.)
cases = [('scaled', scaled, False), ('sheared', sheared, False),
         ('reflected', reflected, False), ('non-affine', non_affine, False),
         ('source-rigid', base, True), ('rotated-translated', rotated, True)]
results, failures = [], []
for name, matrix, accepted in cases:
    case_worlds = copy.deepcopy(worlds)
    target = next(row for row in case_worlds if row['identity'] == emitter['identity'])
    target['candidate_world_matrix_row_major'] = [list(row) for row in matrix]
    world_path = args.output / (name + '-world.json')
    world_path.write_text(json.dumps(case_worlds), encoding='utf-8')
    output_path = args.output / (name + '-asset')
    arguments = original.copy()
    for key, replacement in [('inventory', inventory_path), ('world', world_path), ('output', output_path)]:
        arguments[arguments.index('--' + key) + 1] = str(replacement)
    arguments += ['--resolution', '8', '--sample-rate', '1']
    sys.argv = ['spline-input-fixture', '--', *arguments]
    try:
        builder.main()
    except ValueError as error:
        correct_error = 'proper rigid world matrix' in str(error)
        result = {'case': name, 'accepted': False, 'error': str(error)}
        if accepted or not correct_error:
            failures.append(f'{name}: unexpected rejection: {error}')
    else:
        result = {'case': name, 'accepted': True}
        if not accepted:
            failures.append(f'{name}: non-rigid world input silently accepted')
        elif not (output_path / 'receipt.json').is_file():
            failures.append(f'{name}: accepted conversion lacks a receipt')
    results.append(result)
(args.output / 'result.json').write_text(json.dumps({'results': results, 'failures': failures}, indent=2),
                                        encoding='utf-8')
assert not failures, '\n'.join(failures)
print('PASS: source spline worlds require proper rigid transforms')
