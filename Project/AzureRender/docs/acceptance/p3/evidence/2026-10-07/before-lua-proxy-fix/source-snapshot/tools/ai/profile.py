"""Expose tool-owned profile catalogs without provider credentials."""
def profiles(config, fixed):
    if fixed:
        return [dict(id='content',provider='fixed'),dict(id='general',provider='fixed')]
    return config['profiles']

def resolve_profile(catalog, identity):
    for row in catalog:
        if row['id']==identity:
            return row
    raise ValueError('Unknown profile')
