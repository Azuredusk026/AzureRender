"""Resident changes are evidence of residency, not allocation ownership."""
import unittest
from collect_memory_residency import summarize_residency, AllocationLedger


def snapshot(*regions):
    return {'regions':list(regions)}


def region(base,resident,kind=0x20000,path=''):
    return dict(allocation=base,kind=kind,path=path,residentBytes=resident)


class ResidencyTests(unittest.TestCase):
    def test_image_and_private_residency_are_distinguished(self):
        change=summarize_residency(snapshot(region(1,4096,0x1000000,'driver.dll'),region(2,4096)),
            snapshot(region(1,8192,0x1000000,'driver.dll'),region(2,12288)))
        self.assertEqual(change['byClass']['image'],4096)
        self.assertEqual(change['byClass']['privateUnattributed'],8192)
        self.assertFalse(change['allocationOwnershipProven'])

    def test_disappearing_resident_region_does_not_prove_release(self):
        change=summarize_residency(snapshot(region(7,4096)),snapshot())
        self.assertEqual(change['netResidentChangeBytes'],-4096)
        self.assertEqual(change['allocationReleaseProof'],[])

    def test_reused_address_with_different_mapping_has_distinct_identity(self):
        change=summarize_residency(snapshot(region(4,8192,0x1000000,'first.dll')),
            snapshot(region(4,4096,0x1000000,'second.dll')))
        self.assertEqual(change['netResidentChangeBytes'],-4096)
        self.assertEqual(len(change['regions']),2)

    def test_unknown_allocation_identity_remains_unattributed(self):
        change=summarize_residency(snapshot(region(4,4096)),snapshot(region(None,4096,0)))
        self.assertEqual(change['byClass']['unknown'],4096)
        self.assertFalse(change['allocationOwnershipProven'])

    def test_failed_allocation_and_failed_free_do_not_change_ledger(self):
        ledger=AllocationLedger()
        ledger.observe(dict(action='allocate',success=False,base=1,size=4096))
        self.assertEqual(ledger.live_bytes,0)
        ledger.observe(dict(action='allocate',success=True,base=1,size=4096))
        ledger.observe(dict(action='release',success=False,base=1,size=0))
        self.assertEqual(ledger.live_bytes,4096)
        ledger.observe(dict(action='release',success=True,base=1,size=0))
        self.assertEqual(ledger.live_bytes,0)

    def test_partial_coverage_and_overlapping_wrappers_are_explicit(self):
        ledger=AllocationLedger()
        ledger.observe(dict(action='allocate',success=True,base=1,size=4096))
        ledger.observe(dict(action='allocate',success=True,base=1,size=4096))
        ledger.observe(dict(action='release',success=True,base=99,size=0))
        self.assertEqual(ledger.live_bytes,4096)
        self.assertEqual(ledger.ambiguous_events,2)


if __name__=='__main__': unittest.main()
