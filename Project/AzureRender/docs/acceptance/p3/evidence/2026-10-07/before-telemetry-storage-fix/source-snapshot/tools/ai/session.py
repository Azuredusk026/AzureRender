"""Bounded profile leases; chat requests carry their own stateless history."""
from .profile import resolve_profile

class SessionRegistry:
    def __init__(self):
        self.entries={}
        self.sequence=0
    def create(self, catalog, profile):
        resolve_profile(catalog,profile)
        if len(self.entries)>=64:
            raise ValueError('Session budget exceeded')
        self.sequence+=1
        identity='session-'+str(self.sequence)
        self.entries[identity]=profile
        return identity
    def close(self, identity):
        if not isinstance(identity,str) or identity not in self.entries:
            raise ValueError('Unknown session')
        del self.entries[identity]
    def clear(self):
        self.entries.clear()
