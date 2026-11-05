"""Validate indexed GPU fixtures or an explicit fixed-binding request."""
import re


def require_texture_access(log, *, fixed_requested=False):
    match = re.search(
        r"Character texture access: (fixed|indexed), requested slots=(\d+), stage sampler limit=(\d+)",
        log)
    assert match, "Missing texture access and sampler-capacity diagnostics"
    mode, slots, limit = match.group(1), int(match.group(2)), int(match.group(3))
    assert slots > 0 and limit > 0, "Invalid texture access or sampler capacity"
    # Material slots alone exceeding the stage limit prove indexed access cannot
    # fit. Other fixed fallbacks remain failures for these indexed fixtures.
    capacity_requires_fixed = slots > limit
    expected = "fixed" if fixed_requested or capacity_requires_fixed else "indexed"
    assert mode == expected, f"Texture access: expected {expected}, got {mode}; slots={slots}, limit={limit}"
    return dict(mode=mode, requestedSlots=slots, stageSamplerLimit=limit,
                fixedRequested=fixed_requested, capacityRequiresFixed=capacity_requires_fixed)
