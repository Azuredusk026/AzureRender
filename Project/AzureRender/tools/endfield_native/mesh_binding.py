"""Resolve exact exported container pointers; retain importer-name aliases."""


def resolve_mesh_binding(reference, containers, exports):
    if not isinstance(reference, str) or not reference or reference.count('##') > 1:
        raise ValueError('invalid source mesh reference')
    path, separator, label = reference.partition('##')
    if not path or separator and not label:
        raise ValueError('invalid source mesh subasset label')
    asset = containers.get(path.casefold())
    if (not isinstance(asset, dict) or asset.get('Type') != 'Mesh'
            or not isinstance(asset.get('Source'), str) or not asset['Source']
            or type(asset.get('PathId')) is not int
            or not isinstance(asset.get('Name'), str) or not asset['Name']):
        raise ValueError('source container does not bind a concrete mesh pointer: ' + path)
    matches = [x for x in exports if (x.get('Asset', {}).get('Source'),
               x.get('Asset', {}).get('PathId')) == (asset['Source'], asset['PathId'])]
    if (len(matches) != 1 or not isinstance(matches[0].get('File'), str)
            or not matches[0]['File'] or matches[0]['Asset'] != asset):
        raise ValueError('source mesh pointer lacks a unique matching export: ' + path)
    alias = dict(requested=label, bound=asset['Name']) if separator and label.casefold() != asset['Name'].casefold() else None
    return asset, matches[0], alias
