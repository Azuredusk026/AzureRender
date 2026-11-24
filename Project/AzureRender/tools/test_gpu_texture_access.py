"""Keep GPU path checks strict while recognizing demonstrated sampler limits."""
import unittest

try:
    from gpu_texture_access import require_texture_access
except ImportError:
    require_texture_access = None


class TextureAccessTests(unittest.TestCase):
    def check(self, mode, slots, limit, fixed=False):
        self.assertIsNotNone(require_texture_access, "GPU texture access checker is missing")
        return require_texture_access(
            f"Character texture access: {mode}, requested slots={slots}, stage sampler limit={limit}",
            fixed_requested=fixed)

    def test_indexed_within_capacity(self):
        self.assertEqual(self.check("indexed", 18, 64)["mode"], "indexed")

    def test_capacity_requires_fixed(self):
        self.assertEqual(self.check("fixed", 74, 64)["mode"], "fixed")

    def test_explicit_fixed(self):
        self.assertEqual(self.check("fixed", 18, 64, True)["mode"], "fixed")

    def test_unexplained_fixed_is_rejected(self):
        with self.assertRaisesRegex(AssertionError, "expected indexed"):
            self.check("fixed", 18, 64)

    def test_over_capacity_indexed_is_rejected(self):
        with self.assertRaisesRegex(AssertionError, "expected fixed"):
            self.check("indexed", 74, 64)

    def test_ignored_fixed_request_is_rejected(self):
        with self.assertRaisesRegex(AssertionError, "expected fixed"):
            self.check("indexed", 18, 64, True)

    def test_missing_or_invalid_diagnostics_are_rejected(self):
        self.assertIsNotNone(require_texture_access, "GPU texture access checker is missing")
        for text in ("", "Character texture access: unsupported", "requested slots=18"):
            with self.subTest(text=text), self.assertRaises(AssertionError):
                require_texture_access(text)
        for slots, limit in ((0, 64), (18, 0), (-1, 64)):
            with self.subTest(slots=slots, limit=limit), self.assertRaises(AssertionError):
                self.check("fixed", slots, limit)


if __name__ == "__main__":
    unittest.main()
