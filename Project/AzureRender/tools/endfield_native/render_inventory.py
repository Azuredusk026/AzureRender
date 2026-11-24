"""Conservative evidence inventory for original render objects."""


def build_inventory(game_version, objects, unresolved_dependencies):
    seen = set()
    unknown = []
    for obj in objects:
        identity = obj.get('identity')
        if (not isinstance(identity, (list, tuple)) or len(identity) != 2
                or not isinstance(identity[0], str) or not identity[0]
                or type(identity[1]) is not int):
            raise ValueError('invalid object identity')
        key = tuple(identity)
        if key in seen:
            raise ValueError(f'duplicate object identity: {key}')
        seen.add(key)
        if obj.get('activation') not in ('active', 'inactive', 'unverified'):
            raise ValueError(f'invalid activation: {key}')
        if obj['activation'] == 'unverified':
            unknown.append(list(key))
    return {'format': 'NativeRenderInventory/1', 'game_version': game_version,
            'status': 'incomplete' if unknown or unresolved_dependencies else 'resolved',
            'objects': objects, 'unverified_objects': unknown,
            'unresolved_dependencies': unresolved_dependencies}
