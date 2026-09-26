# Render Core R2-R6/E7 Implementation Plan

> **For agentic workers:** Implement this plan task by task. Each task ends with a test, documentation update, and an independent commit.

**Goal:** Complete R2 through R6/E7 for the Windows Render Core while keeping Android Deferred, then provide the evidence required before G0.

**Architecture:** Extend the current CPU RenderGraph into a declaration and compilation layer for pass ordering, resource states, transient allocation, and frame ownership. Migrate the public frame path incrementally, then add compute/material quality, lighting/shadow, parallel submission, and Windows platform validation behind the existing RHI and NullRHI boundaries.

**Tech Stack:** C++17, Vulkan 1.3, GLFW, VMA, CMake/Ninja, MSVC, NullRHI, Python validation tools.

**Spec:** `docs/plans/azure-engine-plan.md`

## Global Constraints

- Windows is the only active target; Android remains `Deferred`.
- Render Core work completes before G0 engine module work.
- Every public interface change has a runtime document and a contract test.
- Every phase has a separate `feat(<phase>): <中文摘要>` commit.
- Debug and Release CTest, visual regression, smoke runs, documentation checks, and release gate are required at each phase boundary.

## Review Focus

- Resource read/write hazards must produce deterministic order and diagnostic errors.
- Persistent and transient resources must never alias while an in-flight frame can still read them.
- Descriptor fallback and bindless paths must render the same public scenes.
- Parallel recording must preserve deterministic pass order and resource ownership.
- Window resize, minimization, device loss, and repeated swapchain recreation must remain recoverable.

### Task 1: R2 resource states and graph barriers

**Files:** `src/render/RenderGraph.*`, `src/rhi/Rhi.hpp`, `src/render/RenderGraphResources.*`, `tests/RenderGraphTests.cpp`, `tests/NullRhiTests.cpp`, `docs/runtime/render-graph*.md`.

- [ ] Add resource usage declarations with image layout, pipeline stage, and access masks.
- [ ] Compile per-resource state transitions and expose barrier batches.
- [ ] Test write-after-read, read-after-write, duplicate writes, invalid handles, and deterministic diagnostics.
- [ ] Connect compiled barriers to NullRHI and Vulkan command recording.
- [ ] Run Debug/Release tests and commit `feat(r2): 接入渲染图资源状态`.

### Task 2: R2 transient pool and frame ownership

**Files:** `src/render/TransientResourcePool.*`, `src/render/FrameResourceRegistry.*`, `tests/TransientResourcePoolTests.cpp`, `docs/runtime/render-graph-resources.md`.

- [ ] Define persistent, transient, external, and capture resource classes.
- [ ] Reuse transient resources only after the owning frame fence is complete.
- [ ] Test alias prevention, resize invalidation, and shutdown cleanup.
- [ ] Commit `feat(r2): 建立渲染图资源池`.

### Task 3: R2 public frame migration

**Files:** `src/app/AzureRenderFrame.cpp`, `src/render/RenderContext.*`, `src/extensions/ISceneRenderer.hpp`, scene renderers, `tests/NullRhiPassTests.cpp`.

- [ ] Register shadow, scene, post-process, capture, and editor UI passes.
- [ ] Preserve scene renderer extension boundaries and existing visual outputs.
- [ ] Verify all three scenes, both descriptor paths, captures, and historical frames.
- [ ] Commit `feat(r2): 接入公共帧渲染图`.

### Task 4: R3 compute and material quality

**Files:** `src/render/ComputePass.*`, material and shader files, asset conversion tools, tests and runtime docs.

- [ ] Add capability-gated compute dispatch and fallback paths.
- [ ] Add IBL and bloom resources, compute skinning/morph interfaces, and OpenEXR import validation.
- [ ] Verify public assets, bindless/fixed-table parity, and Windows performance budget.
- [ ] Commit `feat(r3): 建立计算与材质质量管线`.

### Task 5: R4 lighting and shadows

**Files:** lighting scene data, shadow graph passes, shaders, NullRHI tests, runtime docs.

- [ ] Add light buffer ownership and deterministic light selection.
- [ ] Add cascaded shadow or clustered-lighting baseline with capability fallback.
- [ ] Verify occlusion, multiple lights, shadow stability, and timing reports.
- [ ] Commit `feat(r4): 建立光照与阴影体系`.

### Task 6: R5 parallel submission

**Files:** frame snapshot, task scheduler, command recording, indirect draw data, tests and diagnostics.

- [ ] Freeze immutable frame snapshots before worker recording.
- [ ] Record independent graph passes in parallel while submitting in compiled order.
- [ ] Add indirect draw path with deterministic single-thread fallback.
- [ ] Compare CPU/GPU timings, draw counts, memory, and image hashes.
- [ ] Commit `feat(r5): 接入并行与 GPU 提交`.

### Task 7: R6/E7 Windows Render Core acceptance

**Files:** acceptance scripts, public assets, install/release tooling, runtime and acceptance docs.

- [ ] Run complex public scenes, quality tiers, resize/minimize/recovery, and long-running smoke tests.
- [ ] Run Debug Validation and Release install/package gates.
- [ ] Record machine-readable performance, visual, and environment evidence.
- [ ] Mark R2-R6/E7 Complete only after all evidence is present.
- [ ] Commit `feat(r6): 完成 Windows 渲染核心验收`.
