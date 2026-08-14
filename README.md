# AzureRender

AzureRender is a C++17/Vulkan stylized character renderer and the implementation workspace for the FYP:

> Comparative Evaluation of Vulkan Subpasses and Dynamic Rendering Local Read for Real-Time NPR Rendering

The active application is in [`Project/AzureRender`](Project/AzureRender). M2 and
AR-1 through AR-4.1 are complete; the active work package is AR-4.2 runtime
resource location. M3/SC and the Multi-pass/Subpass/DRLR benchmark are deferred
while the renderer/editor release path advances toward AR-4.5.

Start here:

- [`Project/AzureRender/docs/ACTIVE_DEVELOPMENT_PLAN_CN.md`](Project/AzureRender/docs/ACTIVE_DEVELOPMENT_PLAN_CN.md) — authoritative near-term queue, task exits and commit titles;
- [`MASTER_DEVELOPMENT_PLAN_CN.md`](MASTER_DEVELOPMENT_PLAN_CN.md) — long-term research roadmap and dependencies;
- [`Project/AzureRender/README.md`](Project/AzureRender/README.md) — build, run and controls;
- [`Project/AzureRender/docs/PROJECT_HANDOFF_CN.md`](Project/AzureRender/docs/PROJECT_HANDOFF_CN.md) — actual engineering state and recovery entry;
- [`DEVELOPMENT_ENVIRONMENT_CN.md`](DEVELOPMENT_ENVIRONMENT_CN.md) — pinned tools, dependencies and regression commands.

## Private asset notice

This private repository intentionally contains third-party test assets under `Project/AzureRender/assets_private`. Access must only be granted to trusted collaborators. Repository privacy does not replace the original asset license; do not publish, redistribute or change the repository to public without a separate license review.

`Project/Vulkan-Tutorial` is a fixed-version Git submodule. `AfterglowRender/`
is an optional local reference directory and is intentionally ignored by this
repository. Clone the tracked workspace with:

```powershell
git clone --recurse-submodules https://github.com/Azuredusk026/AzureRender.git
```
