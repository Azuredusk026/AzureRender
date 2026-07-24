# AzureRender

AzureRender is a C++17/Vulkan stylized character renderer and the implementation workspace for the FYP:

> Comparative Evaluation of Vulkan Subpasses and Dynamic Rendering Local Read for Real-Time NPR Rendering

The active application is in [`Project/MyVulkanApp`](Project/MyVulkanApp). The current implementation has completed the S21 portfolio renderer milestone; the deferred Multi-pass/Subpass/DRLR benchmark remains future work.

Start here:

- [`Project/MyVulkanApp/README.md`](Project/MyVulkanApp/README.md) — build, run and controls;
- [`Project/MyVulkanApp/docs/PROJECT_HANDOFF_CN.md`](Project/MyVulkanApp/docs/PROJECT_HANDOFF_CN.md) — actual status and next steps;
- [`DEVELOPMENT_ENVIRONMENT_CN.md`](DEVELOPMENT_ENVIRONMENT_CN.md) — pinned tools, dependencies and regression commands.

## Private asset notice

This private repository intentionally contains third-party test assets under `Project/MyVulkanApp/assets_private`. Access must only be granted to trusted collaborators. Repository privacy does not replace the original asset license; do not publish, redistribute or change the repository to public without a separate license review.

`AfterglowRender` and `Project/Vulkan-Tutorial` are fixed-version Git submodules used as references. Clone with:

```powershell
git clone --recurse-submodules https://github.com/Azuredusk026/AzureRender.git
```
