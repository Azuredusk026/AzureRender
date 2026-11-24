"""Validate read-only render references; report missing capture gates explicitly."""
import argparse
import hashlib
import json
import math
import re
from pathlib import Path

from PIL import Image, UnidentifiedImageError


def inspect_reference_image(path):
    """Read actual still-image encoding and pixels without resizing the source."""
    try:
        with Image.open(path) as image:
            metadata = {'format': image.format, 'resolution': list(image.size)}
            image.verify()
        with Image.open(path) as image:
            image.load()
    except (OSError, ValueError, SyntaxError, UnidentifiedImageError,
            Image.DecompressionBombError) as error:
        raise ValueError(f'invalid reference image: {path}') from error
    return metadata


def _number(value):
    return type(value) in (int, float) and math.isfinite(value)


def _reproduction(entry, root):
    if entry['role'] != 'engine_baseline':
        raise ValueError('reproduction requires engine_baseline role')
    recipe = entry['reproduction']
    fields = {'schemaVersion', 'workingDirectory', 'program', 'project', 'inputs', 'command'}
    if not isinstance(recipe, dict) or set(recipe) != fields:
        raise ValueError('reproduction fields are incomplete or unknown')
    if type(recipe['schemaVersion']) is not int or recipe['schemaVersion'] != 1:
        raise ValueError('unsupported reproduction schemaVersion')
    folder = recipe['workingDirectory']
    if not isinstance(folder, str) or not folder.strip():
        raise ValueError('invalid reproduction workingDirectory')
    folder = (Path(root) / folder).resolve()
    if not folder.is_dir():
        raise ValueError('reproduction workingDirectory is missing')
    inputs = recipe['inputs']
    if not isinstance(inputs, list) or not inputs:
        raise ValueError('reproduction inputs must be nonempty')
    seen = set()

    def check_file(record, role):
        if not isinstance(record, dict) or set(record) != {'path', 'bytes', 'sha256'}:
            raise ValueError(f'invalid reproduction {role} file record')
        path, size, expected = record['path'], record['bytes'], record['sha256']
        if (not isinstance(path, str) or not path.strip() or type(size) is not int or size < 0
                or not isinstance(expected, str) or not re.fullmatch('[0-9a-f]{64}', expected)):
            raise ValueError(f'invalid reproduction {role} file identity')
        path = (Path(root) / path).resolve()
        if path in seen:
            raise ValueError(f'reproduction duplicate file: {path}')
        seen.add(path)
        if not path.is_file() or path.stat().st_size != size:
            raise ValueError(f'reproduction {role} file missing or size mismatch: {path}')
        digest = hashlib.sha256()
        with path.open('rb') as stream:
            for block in iter(lambda: stream.read(1024 * 1024), b''):
                digest.update(block)
        if digest.hexdigest() != expected:
            raise ValueError(f'reproduction {role} sha256 mismatch: {path}')
        return path

    program = check_file(recipe['program'], 'program')
    project = check_file(recipe['project'], 'project')
    for record in inputs:
        check_file(record, 'input')
    command = recipe['command']
    if (not isinstance(command, list) or len(command) < 3
            or any(not isinstance(x, str) or not x.strip() or '\0' in x for x in command)
            or command.count('--project') != 1 or command[-1] == '--project'):
        raise ValueError('invalid reproduction command; one --project is required')
    if (folder / command[0]).resolve() != program:
        raise ValueError('reproduction command program does not match locked program')
    if (folder / command[command.index('--project') + 1]).resolve() != project:
        raise ValueError('reproduction command project does not match locked project')


def _state(state):
    required = {'scene', 'quest', 'lighting', 'quality', 'resolution',
                'camera', 'exposure', 'antialiasing', 'time'}
    if not isinstance(state, dict) or not required <= state.keys():
        raise ValueError('reference state is incomplete')
    for key in ('scene', 'quest', 'lighting', 'quality', 'antialiasing'):
        if not isinstance(state[key], str) or not state[key].strip():
            raise ValueError(f'invalid state.{key}')
    resolution = state['resolution']
    if not isinstance(resolution, list) or len(resolution) != 2 or any(
            type(x) is not int or x <= 0 for x in resolution):
        raise ValueError('invalid state.resolution')
    if not all(_number(state[k]) for k in ('exposure', 'time')):
        raise ValueError('invalid state exposure/time')
    camera = state['camera']
    if not isinstance(camera, dict):
        raise ValueError('invalid camera')
    for key, length in (('position', 3), ('rotation', 4)):
        vector = camera.get(key)
        if not isinstance(vector, list) or len(vector) != length or not all(map(_number, vector)):
            raise ValueError(f'invalid camera.{key}')
    if abs(sum(x*x for x in camera['rotation']) - 1) > 1e-4:
        raise ValueError('camera rotation must be a unit quaternion')
    fov = camera.get('fov_y')
    if not _number(fov) or not 0 < fov < 180:
        raise ValueError('invalid camera.fov_y')


def validate_manifest(document, root):
    """Return eligible, pending and auxiliary IDs without altering referenced files.

    Paths resolve relative to the manifest. Absolute paths allow local evidence
    archives to be referenced without copying large recordings.
    """
    if document.get('format') != 'NativeRenderReference/1':
        raise ValueError('unsupported reference format')
    version = document.get('game_version')
    if not isinstance(version, str) or not version:
        raise ValueError('missing game version')
    entries = document.get('entries')
    if not isinstance(entries, list) or not entries:
        raise ValueError('reference entries must be nonempty')
    result = {'eligible': [], 'pending': [], 'auxiliary': []}
    seen = set()
    for entry in entries:
        identity = entry.get('id')
        if not isinstance(identity, str) or not identity or identity in seen:
            raise ValueError('missing or duplicate reference identity')
        seen.add(identity)
        role = entry.get('role')
        if role not in ('game_reference', 'engine_baseline', 'network_reference'):
            raise ValueError('invalid reference role')
        if entry.get('status') == 'missing':
            if not isinstance(entry.get('reason'), str) or not entry['reason'].strip():
                raise ValueError('missing reference requires reason')
            result['pending'].append(identity)
            continue
        observed = entry.get('status') == 'observed'
        if observed and (role != 'game_reference' or not isinstance(entry.get('reason'), str)
                         or not entry['reason'].strip()):
            raise ValueError('observed game reference requires pending calibration reason')
        if entry.get('status') not in ('captured', 'observed'):
            raise ValueError('invalid reference status')
        if entry.get('game_version') != version:
            raise ValueError('reference version mismatch')
        if not isinstance(entry.get('source'), str) or not entry['source'].strip():
            raise ValueError('reference source is required')
        if role == 'game_reference' and not observed:
            _state(entry.get('state'))
        path = entry.get('path')
        if not isinstance(path, str) or not path:
            raise ValueError('reference path is required')
        path = Path(root) / path
        if not path.is_file():
            raise ValueError(f'reference file missing: {path}')
        digest = hashlib.sha256()
        with path.open('rb') as stream:
            for block in iter(lambda: stream.read(1024 * 1024), b''):
                digest.update(block)
        if digest.hexdigest() != entry.get('sha256'):
            raise ValueError(f'reference sha256 mismatch: {identity}')
        image = inspect_reference_image(path)
        if role == 'game_reference' and not observed:
            if image['resolution'] != entry['state']['resolution']:
                raise ValueError(f'reference image resolution mismatch: {identity}')
        if 'reproduction' in entry:
            _reproduction(entry, root)
        result['pending' if observed else 'eligible' if role == 'game_reference' else 'auxiliary'].append(identity)
    return result


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('manifest', type=Path)
    args = parser.parse_args()
    document = json.loads(args.manifest.read_text(encoding='utf-8-sig'))
    print(json.dumps(validate_manifest(document, args.manifest.resolve().parent), indent=2))


if __name__ == '__main__':
    main()
