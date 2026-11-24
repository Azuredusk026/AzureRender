"""Apply a fingerprinted arithmetic-order policy to a compute SM5 DXBC program.

Token fields follow Microsoft DirectX-Headers at c526f205c5f96cd2884bf0834c14c7e61bb726c8.
Container hashing follows Microsoft's DxilHash.cpp at
84f68ed1a40db3f3444a306162cab6ac0adfe842 (Copyright Microsoft Corporation).
The adapted hashing rules are covered by third_party/shader_arithmetic_order/.
"""
import argparse
from collections.abc import Mapping
import hashlib
import json
import math
from pathlib import Path
import struct


_MASK = 0xffffffff
_LIMIT = 16 * 1024 * 1024
_MUL, _MAD, _DP3, _CUSTOM_DATA = 56, 50, 16, 53
_SHIFTS = ([7, 12, 17, 22] * 4 + [5, 9, 14, 20] * 4
           + [4, 11, 16, 23] * 4 + [6, 10, 15, 21] * 4)
_CONSTANTS = tuple(int(abs(math.sin(i + 1)) * 2**32) for i in range(64))


def _checksum(payload):
    """Use the official retail container padding with the MD5 compression rounds."""
    length = len(payload)
    remainder = length % 64
    whole = length - remainder
    tail = payload[whole:]
    if remainder < 56:
        tail = (struct.pack("<I", length * 8) + tail + b"\x80"
                + bytes(55 - remainder) + struct.pack("<I", (length << 1) | 1))
    else:
        tail = (tail + b"\x80" + bytes(63 - remainder)
                + struct.pack("<I", length * 8) + bytes(56)
                + struct.pack("<I", (length << 1) | 1))
    padded = payload[:whole] + tail
    state = [0x67452301, 0xefcdab89, 0x98badcfe, 0x10325476]
    for offset in range(0, len(padded), 64):
        words = struct.unpack_from("<16I", padded, offset)
        a, b, c, d = state
        for i in range(64):
            if i < 16:
                f, index = (b & c) | (~b & d), i
            elif i < 32:
                f, index = (d & b) | (~d & c), (5 * i + 1) % 16
            elif i < 48:
                f, index = b ^ c ^ d, (3 * i + 5) % 16
            else:
                f, index = c ^ (b | ~d), (7 * i) % 16
            value = (a + f + _CONSTANTS[i] + words[index]) & _MASK
            shift = _SHIFTS[i]
            rotated = ((value << shift) | (value >> (32 - shift))) & _MASK
            a, d, c, b = d, c, b, (b + rotated) & _MASK
        state = [(old + new) & _MASK for old, new in zip(state, (a, b, c, d))]
    return struct.pack("<4I", *state)


def _container(data):
    if not isinstance(data, bytes) or not 32 <= len(data) <= _LIMIT:
        raise ValueError("Invalid DXBC container length")
    if data[:4] != b"DXBC":
        raise ValueError("Invalid DXBC container magic")
    version, length, count = struct.unpack_from("<3I", data, 20)
    if version != 1 or length != len(data) or not 1 <= count <= 4096:
        raise ValueError("Invalid DXBC container header")
    table_end = 32 + 4 * count
    if table_end > length:
        raise ValueError("Invalid DXBC chunk table length")
    chunks = []
    ranges = []
    for index in range(count):
        start = struct.unpack_from("<I", data, 32 + 4 * index)[0]
        if start % 4 or start < table_end or start + 8 > length:
            raise ValueError("Invalid DXBC chunk offset")
        size = struct.unpack_from("<I", data, start + 4)[0]
        end = start + 8 + size
        if end > length:
            raise ValueError("Invalid DXBC chunk length")
        ranges.append((start, end))
        chunks.append((data[start:start + 4], start + 8, end))
    ranges.sort()
    if any(right[0] < left[1] for left, right in zip(ranges, ranges[1:])):
        raise ValueError("Overlapping DXBC chunks")
    if data[4:20] != _checksum(data[20:]):
        raise ValueError("Invalid DXBC checksum")
    code = [entry for entry in chunks if entry[0] in (b"SHEX", b"SHDR")]
    if len(code) != 1:
        raise ValueError("Expected one DXBC shader program")
    return code[0][1:]


def _instructions(code):
    if len(code) < 8 or len(code) % 4:
        raise ValueError("Invalid shader program length")
    words = list(struct.unpack("<%dI" % (len(code) // 4), code))
    if words[0] != 0x00050050 or words[1] != len(words):
        raise ValueError("Expected a compute shader model 5.0 program")
    result = {}
    offset = 2
    while offset < len(words):
        token = words[offset]
        if token & 0x7ff == _CUSTOM_DATA:
            if offset + 1 >= len(words):
                raise ValueError("Missing custom-data length")
            length = words[offset + 1]
            if length < 2:
                raise ValueError("Invalid custom-data length")
        else:
            length = (token >> 24) & 127
        if not length or offset + length > len(words):
            raise ValueError("Invalid instruction length")
        result[offset] = words[offset:offset + length]
        offset += length
    return words, result


def _operands(instruction, opcode, count):
    header = instruction[0]
    if header != opcode | (len(instruction) << 24):
        raise ValueError("Unsupported instruction opcode or control flags")
    result = []
    index = 1
    while index < len(instruction):
        start = index
        token = instruction[index]
        index += 1
        extended = token >> 31
        while extended:
            if index >= len(instruction):
                raise ValueError("Truncated extended operand")
            extension = instruction[index]
            if extension & 63 != 1:
                raise ValueError("Unsupported operand extension")
            extended = extension >> 31
            index += 1
        kind = (token >> 12) & 255
        dimensions = (token >> 20) & 3
        if kind not in (0, 8) or dimensions != (1 if kind == 0 else 2):
            raise ValueError("Expected a direct temporary or constant-buffer operand")
        for dimension in range(dimensions):
            if (token >> (22 + 3 * dimension)) & 7:
                raise ValueError("Relative operands are unsupported")
        index += dimensions
        if index > len(instruction):
            raise ValueError("Truncated operand index")
        operand = instruction[start:index]
        if operand[-1] >= 4096 or (kind == 8 and operand[-2] >= 14):
            raise ValueError("Operand register is outside shader model limits")
        result.append(operand)
    if len(result) != count:
        raise ValueError("Invalid instruction operand count")
    return result


def _destination(operand):
    token = operand[0]
    if ((token >> 12) & 255 or token >> 31 or token & 3 != 2
            or (token >> 2) & 3 != 0 or not (token >> 4) & 15):
        raise ValueError("Expected a temporary destination mask")
    return operand[-1], [i for i in range(4) if token & (1 << (4 + i))]


def _swizzle(operand):
    token = operand[0]
    if token & 3 != 2 or (token >> 2) & 3 != 1:
        raise ValueError("Expected a four-component source swizzle")
    return [(token >> (4 + 2 * i)) & 3 for i in range(4)]


def _reswizzle(operand, order):
    previous = _swizzle(operand)
    result = operand.copy()
    result[0] = (result[0] & ~0xff0) | sum(
        previous[component] << (4 + 2 * i) for i, component in enumerate(order))
    return result


def _encode(opcode, operands):
    words = [word for operand in operands for word in operand]
    return [opcode | ((len(words) + 1) << 24), *words]


def _factors(operands, constant_first):
    result = list(operands)
    kinds = [(operand[0] >> 12) & 255 for operand in result]
    if sorted(kinds) == [0, 8]:
        result.sort(key=lambda operand: ((operand[0] >> 12) & 255 == 8) != constant_first)
    return result


def _reverse_products(multiply, multiply_add):
    first = _operands(multiply, _MUL, 3)
    second = _operands(multiply_add, _MAD, 4)
    temporary, written = _destination(first[0])
    _, destination = _destination(second[0])
    accumulator = second[3]
    if (accumulator[0] >> 12) & 255 or accumulator[-1] != temporary:
        raise ValueError("Multiply-add must accumulate the preceding product")
    selection = _swizzle(accumulator)
    selected = [selection[i] for i in destination]
    if sorted(selected) != written:
        raise ValueError("Accumulator lanes must match the product destination")
    factors = first[1:] + second[1:3]
    if any((operand[0] >> 12) & 255 == 0 and operand[-1] == temporary for operand in factors):
        raise ValueError("Product inputs alias its temporary destination")
    inverse = [0] * 4
    for output in destination:
        inverse[selection[output]] = output
    new_first = [_reswizzle(operand, inverse) for operand in second[1:3]]
    new_second = [_reswizzle(operand, selection) for operand in first[1:]]
    return (_encode(_MUL, [first[0], *_factors(new_first, False)]),
            _encode(_MAD, [second[0], *_factors(new_second, True), accumulator]))


def _policy_operations(policy, code, instructions):
    required = {"schemaVersion", "sourceCodeSha256", "matrixPairs", "dotProducts"}
    if not isinstance(policy, Mapping) or set(policy) != required:
        raise ValueError("Invalid arithmetic-order policy fields")
    if type(policy["schemaVersion"]) is not int or policy["schemaVersion"] != 1:
        raise ValueError("Unsupported arithmetic-order policy version")
    digest = policy["sourceCodeSha256"]
    if not isinstance(digest, str) or digest != hashlib.sha256(code).hexdigest():
        raise ValueError("Shader source fingerprint does not match policy")
    matrices, dots = policy["matrixPairs"], policy["dotProducts"]
    if (not isinstance(matrices, list) or not isinstance(dots, list)
            or len(matrices) + len(dots) > 4096):
        raise ValueError("Invalid arithmetic-order operation list")
    targets = set()

    def target(offset):
        if type(offset) is not int or offset not in instructions:
            raise ValueError("Policy offset is not an instruction boundary")
        if offset in targets:
            raise ValueError("Duplicate or overlapping policy operation")
        targets.add(offset)

    for pair in matrices:
        if not isinstance(pair, Mapping) or set(pair) != {"multiplyOffsetWords", "multiplyAddOffsetWords"}:
            raise ValueError("Invalid matrix-pair policy fields")
        multiply, add = pair["multiplyOffsetWords"], pair["multiplyAddOffsetWords"]
        target(multiply)
        target(add)
        if add != multiply + len(instructions[multiply]):
            raise ValueError("Matrix-pair instructions must be adjacent")
    for dot in dots:
        if not isinstance(dot, Mapping) or set(dot) != {"offsetWords", "componentOrder"}:
            raise ValueError("Invalid dot-product policy fields")
        target(dot["offsetWords"])
        order = dot["componentOrder"]
        if (not isinstance(order, list) or len(order) != 4
                or any(type(i) is not int or not 0 <= i <= 3 for i in order)
                or sorted(order[:3]) != [0, 1, 2]):
            raise ValueError("Invalid dot-product component permutation")
    return matrices, dots


def normalize_shader(data, policy):
    """Return a checked program with the policy's product and dot order applied."""
    start, end = _container(data)
    code = data[start:end]
    words, instructions = _instructions(code)
    matrices, dots = _policy_operations(policy, code, instructions)
    for pair in matrices:
        a, b = pair["multiplyOffsetWords"], pair["multiplyAddOffsetWords"]
        first, second = _reverse_products(instructions[a], instructions[b])
        pair_end = b + len(instructions[b])
        if len(first) + len(second) != pair_end - a:
            raise ValueError("Arithmetic-order policy changes the pair length")
        words[a:pair_end] = first + second
    for dot in dots:
        offset = dot["offsetWords"]
        operands = _operands(instructions[offset], _DP3, 3)
        _destination(operands[0])
        result = _encode(_DP3, [operands[0], *[
            _reswizzle(operand, dot["componentOrder"]) for operand in operands[1:]]])
        if len(result) != len(instructions[offset]):
            raise ValueError("Dot-product policy changes instruction length")
        words[offset:offset + len(result)] = result
    result = bytearray(data)
    result[start:end] = struct.pack("<%dI" % len(words), *words)
    result[4:20] = _checksum(result[20:])
    return bytes(result)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--input", type=Path, required=True)
    parser.add_argument("--policy", type=Path, required=True)
    parser.add_argument("--output", type=Path, required=True)
    args = parser.parse_args()
    source = args.input.read_bytes()
    policy_bytes = args.policy.read_bytes()
    result = normalize_shader(source, json.loads(policy_bytes.decode("utf-8-sig")))
    with args.output.open("xb") as output:
        output.write(result)
    print(json.dumps(dict(status="written", inputSha256=hashlib.sha256(source).hexdigest(),
                          outputSha256=hashlib.sha256(result).hexdigest(),
                          policySha256=hashlib.sha256(policy_bytes).hexdigest())))


if __name__ == "__main__":
    main()
