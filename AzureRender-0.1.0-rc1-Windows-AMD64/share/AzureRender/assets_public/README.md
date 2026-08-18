# Public test assets

`test_model.gltf` is a deliberately tiny, self-contained glTF 2.0 cube used to
exercise the renderer's file-loading, vertex/index, normal, generated tangent,
UV, PNG decoding, texture upload, sampler, normal-map, alpha-mode, double-sided,
and multi-material descriptor paths.

The geometry and 2x2 test texture are embedded as data URIs. This file was
created for this project and may be redistributed with the renderer.

`toon_ramp_profiles.json` is the versioned, renderer-owned CQ-2 ramp source.
`toon_ramp_atlas.ppm` is its generated linear RGB sampling atlas. Regenerate or
validate it with `python tools/build_toon_ramp_atlas.py [--check]`.
