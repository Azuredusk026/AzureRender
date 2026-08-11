# AzureRender

AzureRender is a C++17/Vulkan stylized character renderer and the implementation workspace for the FYP:

> Comparative Evaluation of Vulkan Subpasses and Dynamic Rendering Local Read for Real-Time NPR Rendering

The active application is in [`Project/AzureRender`](Project/AzureRender). The current engineering baseline is S36.2; CQ-0 visual QA and CQ-1 Material Classes/Data v1 are complete, and the active work package is M1/CQ-2 Toon Ramp/Shadow. The deferred Multi-pass/Subpass/DRLR benchmark begins only after the portfolio release gate.

Start here:

- [`MASTER_DEVELOPMENT_PLAN_CN.md`](MASTER_DEVELOPMENT_PLAN_CN.md) — authoritative long-term roadmap, dependencies and exit gates;
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
