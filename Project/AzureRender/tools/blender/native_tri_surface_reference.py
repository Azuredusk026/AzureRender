"""Independent real-arithmetic interpreter for an explicit Lit shader receipt.

The JSON receipt supplies assembly, parameterLayout, and shaderDefaults paths
with SHA-256 hashes. Its assembly firstLine/lastLine select the source span.
Relative evidence paths resolve beside the receipt. Shader bytes and assembly
remain external test inputs. RGB samples arrive after sRGB decoding.
"""
import hashlib
import json
from pathlib import Path
import re
import struct


class NativeTriSurfaceReference:
    """Evaluate retained scalar/vector instructions without Blender imports."""

    def __init__(self, evidence_path):
        self.path = Path(evidence_path).resolve(strict=True)
        self.evidence = json.loads(self.path.read_text(encoding='utf-8-sig'))
        self.inputs = {}
        for key in ('assembly', 'parameterLayout', 'shaderDefaults'):
            entry = self.evidence[key]
            path = Path(entry['path'])
            path = (self.path.parent / path).resolve(strict=True) if not path.is_absolute() else path.resolve(strict=True)
            digest = hashlib.sha256(path.read_bytes()).hexdigest()
            if digest != entry['sha256']:
                raise ValueError(f'Shader evidence SHA-256 mismatch: {key}: {path}')
            self.inputs[key] = dict(path=str(path), sha256=digest)
        assembly = self.evidence['assembly']
        lines = Path(self.inputs['assembly']['path']).read_text(encoding='utf-8-sig').splitlines()
        start, end = assembly['firstLine'], assembly['lastLine']
        if not 1 <= start <= end <= len(lines):
            raise ValueError('Invalid shader assembly evidence line span')
        self.instructions = lines[start - 1:end]
        span_hash = hashlib.sha256(('\n'.join(self.instructions) + '\n').encode()).hexdigest()
        if span_hash != assembly['spanSha256']:
            raise ValueError('Shader assembly span SHA-256 mismatch')
        for instruction in self.instructions:
            operation = instruction.split(' ', 1)[0].removesuffix('_sat')
            if operation not in ('mov', 'add', 'mul', 'mad'):
                raise ValueError(f'Unsupported shader reference operation: {operation}')
        self.layout, self.buffer_bytes = self._read_layout()
        tree = json.loads(Path(self.inputs['shaderDefaults']['path']).read_text(encoding='utf-8-sig'))
        properties = tree['m_ParsedForm']['m_PropInfo']['m_Props']
        self.defaults = {p['m_Name']:[p.get(f'm_DefValue[{i}]', 0.0) for i in range(4)]
                         for p in properties}

    def _read_layout(self):
        entry = self.evidence['parameterLayout']
        data = Path(self.inputs['parameterLayout']['path']).read_bytes()
        buffer_name = entry['buffer'].encode('utf-8')
        cursor = data.index(buffer_name) + len(buffer_name)
        buffer_bytes, count = struct.unpack_from('<II', data, cursor)
        if buffer_bytes != entry['bytes'] or count != len(entry['entries']):
            raise ValueError('Shader constant-buffer dimensions differ from evidence receipt')
        cursor += 8
        layout = []
        for _ in range(count):
            length = struct.unpack_from('<I', data, cursor)[0]
            cursor += 4
            name = data[cursor:cursor + length].decode('utf-8')
            cursor = (cursor + length + 3) & ~3
            words = struct.unpack_from('<6I', data, cursor)
            cursor += 24
            components, offset = words[2], words[5]
            if not (1 <= components <= 4 and offset % 4 == 0 and offset + components * 4 <= buffer_bytes):
                raise ValueError(f'Invalid shader constant-buffer field: {name}')
            layout.append((name, components, offset))
        expected = [(field['name'], field['components'], field['byteOffset']) for field in entry['entries']]
        if layout != expected:
            raise ValueError('Parsed shader parameter layout differs from evidence receipt')
        return layout, buffer_bytes

    def _constants(self, record):
        constants = [[0.0] * 4 for _ in range(self.buffer_bytes // 16)]
        slots = {slot['Property']:slot for slot in record.get('Textures', [])}
        for name, count, offset in self.layout:
            if name in record.get('Floats', {}):
                values = [record['Floats'][name]]
            elif name in record.get('Colors', {}):
                values = record['Colors'][name]
            elif name.endswith('_ST') and name[:-3] in slots:
                slot = slots[name[:-3]]
                values = list(slot.get('Scale', [1, 1])) + list(slot.get('Offset', [0, 0]))
            else:
                values = self.defaults.get(name, [0.0] * 4)
            for component, value in enumerate(values[:count]):
                address = offset // 4 + component
                constants[address // 4][address % 4] = float(value)
        return constants

    def evaluate(self, record, base_rgba, nro_rgba, mask_rgb):
        """Return base RGB, metallic, and roughness from the retained program."""
        constants = self._constants(record)
        registers = {f'r{i}':[0.0] * 4 for i in range(6)}
        registers['r1'][:] = [float(x) for x in base_rgba]
        registers['r2'][2] = float(nro_rgba[2])
        registers['r4'][:3] = [float(x) for x in mask_rgb[:3]]
        lanes = 'xyzw'

        def read(token):
            negative = token.startswith('-')
            token = token.lstrip('-')
            if token.startswith('l('):
                value = [float(x.strip()) for x in token[2:-1].split(',')]
                result = value * 4 if len(value) == 1 else value
            else:
                name, _, swizzle = token.partition('.')
                value = constants[int(name[4:-1])] if name.startswith('cb2[') else registers[name]
                swizzle = swizzle or lanes
                swizzle = swizzle * 4 if len(swizzle) == 1 else swizzle
                result = [value[lanes.index(lane)] for lane in swizzle]
            return [-x for x in result] if negative else result

        for instruction in self.instructions:
            opcode, rest = instruction.split(' ', 1)
            arguments = [x.strip() for x in re.split(r',\s*(?![^()]*\))', rest)]
            destination, *sources = arguments
            values = [read(source) for source in sources]
            operation = opcode.removesuffix('_sat')
            if operation == 'mov':
                result = values[0]
            elif operation == 'add':
                result = [a + b for a, b in zip(*values)]
            elif operation == 'mul':
                result = [a * b for a, b in zip(*values)]
            elif operation == 'mad':
                result = [a * b + c for a, b, c in zip(*values)]
            else:
                raise ValueError(f'Unsupported shader reference operation: {operation}')
            if opcode.endswith('_sat'):
                result = [min(1.0, max(0.0, x)) for x in result]
            target, _, swizzle = destination.partition('.')
            for lane in swizzle or lanes:
                registers[target][lanes.index(lane)] = result[lanes.index(lane)]
        return dict(base=registers['r4'][:3], metallic=registers['r0'][1], roughness=registers['r4'][3])
