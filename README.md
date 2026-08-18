# AzureRender

AzureRender 是一个 C++17/Vulkan 实时渲染器，也是 FYP 研究项目的实现仓库。当前产品包含风格化角色渲染、编辑器、确定性捕获、GPU 诊断，以及可插拔场景渲染器和黑洞演示。

## 从这里开始

1. [文档导航](Project/AzureRender/docs/README_CN.md)：先判断应该读哪一份文档。
2. [项目概览](Project/AzureRender/docs/PROJECT_OVERVIEW_CN.md)：了解当前真实状态、架构和限制。
3. [当前开发计划](Project/AzureRender/docs/ACTIVE_DEVELOPMENT_PLAN_CN.md)：唯一任务队列和验收标准。
4. [开发指南](Project/AzureRender/docs/DEVELOPMENT_GUIDE_CN.md)：配置环境、构建和运行测试。
5. [应用快速使用](Project/AzureRender/README.md)：运行命令、场景选择和常用控制。

不要从归档日志中的“下一步”恢复开发。归档内容只用于追溯，当前任务始终以 `ACTIVE_DEVELOPMENT_PLAN_CN.md` 为准。

## 仓库结构

```text
Project/AzureRender/       主工程
Project/Vulkan-Tutorial/   固定版本的上游参考子模块
AfterglowRender/           可选本地参考，不属于主工程
Project/AzureRender/docs/archive/source-documents/
                          提案和早期规划原始材料
```

克隆时初始化子模块：

```powershell
git clone --recurse-submodules https://github.com/Azuredusk026/AzureRender.git
```

## 资产边界

`Project/AzureRender/assets_private/` 包含不得公开分发的第三方测试资产。仓库保持私有并不替代原始许可；公开代码、发布包或作品集前必须单独检查资产授权。公共回归必须能够只依赖 `assets_public/` 完成。
