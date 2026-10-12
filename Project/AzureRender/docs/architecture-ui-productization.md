# 编辑器 UI 与应用生命周期

应用启动按项目中心、项目加载、编辑器和关闭组织。项目中心在场景与编辑器服务初始化之前运行。启动决策归 `src/main.cpp`，基础窗口呈现归 `ProjectCenterHost.cpp`。

项目中心的任务状态由 `ProjectCenterController` 持有。控件绘制由 `ProjectCenterView` 完成，业务操作调用 `ProjectOpenService`。编辑器继续使用 `EditorSession`、面板注册及现有文档保护。

平台为 GLFW，渲染后端为 Vulkan。UI 使用仓库内 ImGui 1.92.8 docking。中心与编辑器复用这些后端，窗口按生命周期顺序重建。

主题和公共控件归 `editor/ui` 与 `EditorTheme`。各面板持有自己的展示状态，编辑操作通过服务处理。项目与发布操作沿用现有生产契约。

阶段任务见[实施计划](plans/2026-10-12-imgui-productization.md)。实际证据见[启动验证](acceptance/ui1/2026-10-12.md)。
