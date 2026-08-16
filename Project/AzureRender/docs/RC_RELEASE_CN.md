# AzureRender RC 发布说明

> 当前版本：RC1（0.1.0-rc1）
> 审计报告：`docs/RC1_AUDIT_CN.md`

## 系统要求

- Windows 10/11 或现代 Linux x86_64；
- 支持 Vulkan 1.3 的 GPU 与驱动；
- Windows 包携带 GLFW 运行库；Linux 需要系统 Vulkan Loader 与 GLFW 运行库。

## 启动

```bash
AzureRender --version
AzureRender --check-resources
AzureRender --smoke-frames 120
AzureRender --asset share/AzureRender/assets_public/test_model.gltf --create-scene demo.azscene
AzureRender --editor demo.azscene
```

可执行文件会从自身相邻的 `../share/AzureRender` 自动定位 Shader 和公共资产。也可使用
`--resource-root <directory>` 或 `AZURERENDER_RESOURCE_ROOT` 显式覆盖。

## 已知限制

- RC1 仅支持 Windows/Linux 桌面，不包含 Android 和论文三路径；
- 编辑器不包含对象拾取、Gizmo、完整 ECS、资源导入或动态插件；
- Linux 包依赖系统 Vulkan Loader/驱动和 GLFW；
- 私有角色资产、私有截图和视频不包含在发布包中。

发布验收以 `docs/RC0_BASELINE_CN.md` 为准，RC1 审计见 `docs/RC1_AUDIT_CN.md`。
