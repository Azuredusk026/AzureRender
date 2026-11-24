import copy
import unittest
import zlib

from endfield_native.animation import verify_generic_candidate


def fixture():
    h = zlib.crc32(b"Root")
    bindings = [{"path": h, "attribute": i, "type": "Transform", "custom_type": 0} for i in (1, 2)]
    bindings += [{"path": 0, "attribute": i, "type": "Animator", "custom_type": 8} for i in range(14)]
    bindings += [{"path": 0, "attribute": i, "type": "Animator", "custom_type": 0} for i in (101, 102, 103)]
    audit = dict(output_track_count=1, root_track_count=21, float_curve_count=17,
                 bindings=bindings, transform_sub_track_masks=[128, 128, 0, 0, 0, 0, 0, 0],
                 default_indices=[], constant_indices=[], constant_values=[], start=0, stop=1/60)
    track = dict(track=0, translation=[0, 0, 0], rotation=[0, 0, 0, 1], scale=[1, 1, 1])
    transforms = [{"time": i/60, "tracks": [copy.deepcopy(track)]} for i in range(2)]
    roots = [{"time": i/60, "values": [0, 0, 0, 0, 0, 0, 1]*3} for i in range(2)]
    scalars = [{"time": i/60, "values": [0, 0, 0, 0, 0, 0, 1]*2+[1, -1, 1]} for i in range(2)]
    source = {"transforms": [{"path": "actor/Root"}]}
    return audit, source, transforms, roots, scalars


class GenericCandidateTests(unittest.TestCase):
    def test_independent_identity_root_groups(self):
        result = verify_generic_candidate(*fixture())
        self.assertEqual(result["tracks"][0]["source_path"], "actor/Root")
        self.assertEqual(result["frames"], 2)

    def test_nontrivial_root_requires_explicit_solver(self):
        values = fixture()
        values[3][1]["values"][14] = .5
        with self.assertRaisesRegex(ValueError, "root"):
            verify_generic_candidate(*values)

    def test_ambiguous_hash_is_rejected(self):
        values = fixture()
        values[1]["transforms"].append({"path": "other/Root"})
        with self.assertRaisesRegex(ValueError, "ambiguous"):
            verify_generic_candidate(*values)

    def test_animated_scalar_requires_explicit_driver(self):
        values = fixture()
        values[4][1]["values"][-1] = .5
        with self.assertRaisesRegex(ValueError, "scalar"):
            verify_generic_candidate(*values)

    def test_nonunit_rotation_is_rejected(self):
        values = fixture()
        values[2][1]["tracks"][0]["rotation"] = [0, 0, 0, 2]
        with self.assertRaisesRegex(ValueError, "rotation"):
            verify_generic_candidate(*values)


if __name__ == "__main__":
    unittest.main()
