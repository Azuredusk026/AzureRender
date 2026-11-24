"""Select explicit source chunks while preserving authored draw identities."""
from pathlib import PurePosixPath
from copy import deepcopy
import re


def _paths(values):
    if not isinstance(values, (list, tuple)) or not values:
        raise ValueError('source chunk paths must be a nonempty list')
    seen = set()
    for value in values:
        if (not isinstance(value, str) or not value or '\\' in value or ':' in value
                or '\x00' in value or value.startswith('/')
                or '..' in value.split('/') or str(PurePosixPath(value)) != value
                or value in seen):
            raise ValueError('source chunk paths must be unique canonical relative paths')
        seen.add(value)
    return seen


def select_chunk_paths(available, requested=None):
    known = _paths(available)
    chosen = known if requested is None else _paths(requested)
    if not chosen <= known:
        raise ValueError('requested source chunk is absent from the inventory')
    return [path for path in available if path in chosen]


def select_renderer_sources(inventory, requested=None):
    available = [chunk['path'] for chunk in inventory['chunks']]
    selected = select_chunk_paths(available, requested)
    known, chosen = set(available), set(selected)
    rows = inventory['renderers']
    if any(row['instance'].get('source') not in known for row in rows):
        raise ValueError('renderer source is absent from the inventory chunks')
    result = [row for row in rows if row['instance']['source'] in chosen]
    if not result:
        raise ValueError('source selection contains no renderer')
    return result, dict(schemaVersion=1, policy='all' if requested is None else 'explicit',
                       selectedChunks=selected,
                       excludedChunks=[p for p in available if p not in chosen],
                       selectedRenderers=len(result), excludedRenderers=len(rows)-len(result))


def apply_level_source_selection(level, inventory, requested):
    """Mask imported native draw nodes in a separate scene-document snapshot."""
    _, receipt = select_renderer_sources(inventory, requested)
    sources = {}
    for row in inventory['renderers']:
        entity, source = row['instance']['entity_id'], row['instance']['source']
        if type(entity) is not int or entity < 0:
            raise ValueError('invalid source entity identity')
        if entity in sources and sources[entity] != source:
            raise ValueError('conflicting entity source')
        sources[entity] = source
    result = deepcopy(level)
    chosen = set(receipt['selectedChunks'])
    changed, mapped, unmapped = [], 0, set()
    for node in result['nodes']:
        match = re.fullmatch(r'native-(\d+)-draw-\d+', node.get('name', ''))
        if not match or not node.get('resourceId'):
            continue
        entity = int(match[1])
        if entity not in sources:
            unmapped.add(entity)
            continue
        mapped += 1
        if sources[entity] not in chosen:
            renderable = node.setdefault('components', {}).setdefault('azure.renderable',
                dict(type='azure.renderable', version=1, data={}))
            data = renderable.setdefault('data', {})
            if data.get('visible', True):
                data['visible'] = False
                changed.append(node['id'])
    if not mapped:
        raise ValueError('scene has no matching imported draw nodes')
    receipt.update(mappedDrawNodes=mapped, changedNodes=changed,
                   unmappedEntities=sorted(unmapped))
    return result, receipt
