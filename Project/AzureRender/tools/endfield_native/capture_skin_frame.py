"""Compare a captured NPR reserved frame with explicitly ordered source bones.

The caller supplies native Unity inverse bind matrices. Converter geometry
reflects X and must be converted back before calling. No bone identity is
inferred from proximity; position and full-matrix residuals are separate.
"""
import struct
import numpy as np


def compare_skin_frame(instance_bytes, palette_bytes, inverse_binds, instance_index=0):
    if type(instance_index) is not int or instance_index < 0:
        raise ValueError('Invalid instance index')
    offset = instance_index * 256
    if len(instance_bytes) < offset + 256 or len(palette_bytes) % 16:
        raise ValueError('Incomplete instance or palette range')
    flags, = struct.unpack_from('<I', instance_bytes, offset + 76)
    if not flags & 16:
        raise ValueError('Draw does not select a reserved skin frame')
    base, = struct.unpack_from('<I', instance_bytes, offset + 80)
    count = len(inverse_binds)
    if not count or (base + 3 + 3 * count) * 16 > len(palette_bytes):
        raise ValueError('Palette does not cover the declared bones')
    model = np.array(struct.unpack_from('<16f', instance_bytes, offset)).reshape(4, 4).T
    rows = np.frombuffer(palette_bytes, dtype='<f4').reshape(-1, 4)
    frame = np.eye(4); frame[:3] = rows[base:base + 3]
    if not np.isfinite(model).all() or not np.isfinite(frame).all():
        raise ValueError('Nonfinite captured matrix')
    comparisons = []
    for index, value in enumerate(inverse_binds):
        bind = np.asarray(value, dtype=float)
        if bind.shape != (4, 4) or not np.isfinite(bind).all():
            raise ValueError('Invalid inverse bind matrix')
        if not np.allclose(bind[3], [0, 0, 0, 1], atol=1e-6, rtol=0):
            raise ValueError('Inverse bind is not affine; check matrix convention')
        skin = np.eye(4); first = base + 3 + 3 * index
        skin[:3] = rows[first:first + 3]
        if not np.isfinite(skin).all():
            raise ValueError('Nonfinite captured bone')
        try:
            world = model @ skin @ np.linalg.inv(bind)
        except np.linalg.LinAlgError as error:
            raise ValueError('Noninvertible inverse bind matrix') from error
        if not np.isfinite(world).all():
            raise ValueError('Nonfinite reconstructed bone')
        comparisons.append({'index': index, 'world': world.tolist(),
            'position_error': float(np.linalg.norm(world[:3, 3] - frame[:3, 3])),
            'matrix_error': float(np.max(np.abs(world[:3] - frame[:3])))})
    return {'format': 'NativeReservedSkinFrameComparison/1', 'instance_index': instance_index,
        'flags': flags, 'palette_row': base, 'reserved_frame': frame.tolist(),
        'origin': frame[:3, 3].tolist(), 'model_origin': model[:3, 3].tolist(),
        'bones': comparisons, 'scope': 'One captured pose; caller verifies source mesh and bone order'}
