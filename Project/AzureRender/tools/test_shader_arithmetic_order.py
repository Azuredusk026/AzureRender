"""Check DXBC ordering against a frozen GPU oracle and an independent FMA case."""
import copy
import hashlib
import json
from pathlib import Path
import struct
import unittest

try:
    from native_render import shader_arithmetic_order as ordering
except ImportError as error:
    if "shader_arithmetic_order" not in str(error):
        raise
    ordering = None


ROOT = Path(__file__).resolve().parents[1]
FIXTURES = ROOT / "tests/fixtures/shader_arithmetic_order"


def shader_code(data):
    count = struct.unpack_from("<I", data, 28)[0]
    for index in range(count):
        offset = struct.unpack_from("<I", data, 32 + 4 * index)[0]
        if data[offset:offset + 4] in (b"SHEX", b"SHDR"):
            length = struct.unpack_from("<I", data, offset + 4)[0]
            return data[offset + 8:offset + 8 + length]
    raise AssertionError("Fixture has no shader code")


def float32(value):
    return struct.unpack("<f", struct.pack("<f", value))[0]


def rounding_result(data, policy):
    code = shader_code(data)
    words = struct.unpack("<%dI" % (len(code) // 4), code)
    rows = []
    pair = policy["matrixPairs"][0]
    for name in ("multiplyOffsetWords", "multiplyAddOffsetWords"):
        start = pair[name]
        instruction = words[start:start + ((words[start] >> 24) & 127)]
        # This independent fixture has two direct constant-buffer operands.
        assert (instruction[3] >> 12) & 255 == 8
        assert (instruction[6] >> 12) & 255 == 8
        rows.append((instruction[5], instruction[8]))
    values = [4097.0, 4097.0, -16785408.0, 1.0]
    product = float32(values[rows[0][0]] * values[rows[0][1]])
    # These products and sums are exact in binary64 before the binary32 round.
    return float32(values[rows[1][0]] * values[rows[1][1]] + product)


class ShaderArithmeticOrderTests(unittest.TestCase):
    def setUp(self):
        self.assertIsNotNone(ordering, "Shader arithmetic order API is missing")
        self.data = (FIXTURES / "compiled-ray-mirror.dxbc").read_bytes()
        self.policy = json.loads((FIXTURES / "ray-policy.json").read_text())

    def test_frozen_program_matches_independent_gpu_order_control(self):
        result = ordering.normalize_shader(self.data, self.policy)
        expected = (FIXTURES / "ordered-ray-mirror.dxbc").read_bytes()
        self.assertEqual(shader_code(result), shader_code(expected))
        self.assertEqual(self.data, (FIXTURES / "compiled-ray-mirror.dxbc").read_bytes())

    def test_independent_fma_rounding_follows_the_requested_product_order(self):
        data = (FIXTURES / "rounding.dxbc").read_bytes()
        policy = json.loads((FIXTURES / "rounding-policy.json").read_text())
        self.assertEqual(rounding_result(data, policy), 1.0)
        result = ordering.normalize_shader(data, policy)
        self.assertEqual(rounding_result(result, policy), 0.0)

    def test_empty_policy_preserves_a_valid_original_container(self):
        data = (ROOT / "docs/acceptance/nr/evidence/2026-10-29/native-ray-source/event-8702.dxbc").read_bytes()
        policy = dict(schemaVersion=1, sourceCodeSha256=hashlib.sha256(shader_code(data)).hexdigest(),
                      matrixPairs=[], dotProducts=[])
        self.assertEqual(ordering.normalize_shader(data, policy), data)

    def test_official_checksum_fixtures_cover_both_padding_boundaries(self):
        for remainder in (56, 63):
            with self.subTest(remainder=remainder):
                data = (FIXTURES / ("rounding-tail%d.dxbc" % remainder)).read_bytes()
                self.assertEqual((len(data) - 20) % 64, remainder)
                policy = dict(schemaVersion=1, sourceCodeSha256=hashlib.sha256(shader_code(data)).hexdigest(),
                              matrixPairs=[], dotProducts=[])
                self.assertEqual(ordering.normalize_shader(data, policy), data)

    def test_recomputed_container_is_accepted_without_a_second_transform(self):
        result = ordering.normalize_shader(self.data, self.policy)
        policy = dict(schemaVersion=1, sourceCodeSha256=hashlib.sha256(shader_code(result)).hexdigest(),
                      matrixPairs=[], dotProducts=[])
        self.assertEqual(ordering.normalize_shader(result, policy), result)

    def test_non_shader_container_regions_are_preserved(self):
        result = ordering.normalize_shader(self.data, self.policy)
        self.assertEqual(len(result), len(self.data))
        code = shader_code(self.data)
        start = self.data.index(code)
        self.assertEqual(result[:4], self.data[:4])
        self.assertEqual(result[20:start], self.data[20:start])
        self.assertEqual(result[start + len(code):], self.data[start + len(code):])

    def test_corrupt_checksum_is_rejected(self):
        damaged = bytearray(self.data)
        damaged[4] ^= 1
        with self.assertRaisesRegex(ValueError, "checksum"):
            ordering.normalize_shader(bytes(damaged), self.policy)

    def test_unknown_program_is_rejected(self):
        policy = copy.deepcopy(self.policy)
        policy["sourceCodeSha256"] = "0" * 64
        with self.assertRaisesRegex(ValueError, "source|fingerprint|SHA"):
            ordering.normalize_shader(self.data, policy)

    def test_duplicate_operation_is_rejected(self):
        policy = copy.deepcopy(self.policy)
        policy["matrixPairs"].append(policy["matrixPairs"][0].copy())
        with self.assertRaisesRegex(ValueError, "overlap|duplicate"):
            ordering.normalize_shader(self.data, policy)

    def test_offset_inside_an_instruction_is_rejected(self):
        policy = copy.deepcopy(self.policy)
        policy["matrixPairs"][0]["multiplyOffsetWords"] += 1
        with self.assertRaisesRegex(ValueError, "offset|boundary"):
            ordering.normalize_shader(self.data, policy)

    def test_invalid_version_offsets_and_permutations_are_rejected(self):
        mutations = [
            lambda p: p.update(schemaVersion=True),
            lambda p: p.update(schemaVersion=2),
            lambda p: p.update(extra="unsupported"),
            lambda p: p["matrixPairs"][0].update(multiplyOffsetWords=True),
            lambda p: p["matrixPairs"][0].update(multiplyOffsetWords=-1),
            lambda p: p["dotProducts"][0].update(componentOrder=[0, 0, 2, 0]),
            lambda p: p["dotProducts"][0].update(componentOrder=[0, 1, 2, 4]),
        ]
        for mutation in mutations:
            with self.subTest(mutation=mutations.index(mutation)):
                policy = copy.deepcopy(self.policy)
                mutation(policy)
                with self.assertRaises(ValueError):
                    ordering.normalize_shader(self.data, policy)

    def test_malformed_containers_are_rejected(self):
        for data in (b"", self.data[:31], b"FAIL" + self.data[4:], self.data[:-4]):
            with self.subTest(length=len(data)):
                with self.assertRaises(ValueError):
                    ordering.normalize_shader(data, self.policy)


if __name__ == "__main__":
    unittest.main()
