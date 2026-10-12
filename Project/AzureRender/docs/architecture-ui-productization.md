# 编辑器 UI 与应用生命周期

应用启动按项目中心、项目加载、编辑器和关闭组织。项目中心在场景与编辑器服务初始化之前运行。启动决策归 `src/main.cpp`，基础窗口呈现归 `ProjectCenterHost.cpp`。

项目中心的任务状态由 `ProjectCenterController` 持有。控件绘制由 `ProjectCenterView` 完成，业务操作调用 `ProjectOpenService`。编辑器继续使用 `EditorSession`、面板注册及现有文档保护。

平台为 GLFW，渲染后端为 Vulkan。UI 使用仓库内 ImGui 1.92.8 docking。中心与编辑器复用这些后端，窗口按生命周期顺序重建。

主题和公共控件归 `editor/ui` 与 `EditorTheme`。各面板持有自己的展示状态，编辑操作通过服务处理。项目与发布操作沿用现有生产契约。

阶段任务见[实施计划](plans/2026-10-12-imgui-productization.md)。实际证据见[启动验证](acceptance/ui1/2026-10-12.md)。

完整制作流程与错误恢复见[交付验证](acceptance/ui4/2026-10-12.md)。
公共框架按现有面板注册接口逐个迁移业务界面。
公共控件覆盖项目中心、项目、发布、资源和日志面板。
层级搜索与属性控件复用公共状态、尺度和工具栏。
属性数值和引用继续通过类型描述与编辑服务。
视口绘制归 `panels/ViewportPanel.cpp`。
视口工具栏读取相机状态，输入和放置使用既有服务。
平台与 Vulkan 资源继续由编辑宿主管理。
安装入口、面板回放与完整 CPU 回归见[集中交付](acceptance/ui8/2026-10-12.md)。
其他面板保留现有业务与编辑服务。

## 安装、项目与发布

安装根由可执行文件位置解析。
安装标记约束资源搜索范围。
用户配置保存最近记录、偏好和引擎日志。
项目目录保存文档与资产，.azure 保存可重建缓存。

项目格式版本与引擎项目 API 分别声明。
项目 API 版本一支持现有 Schema v1 文档。
项目移动后，最近记录依据项目身份更新路径。

发布服务调用安装树附带的 Python 与工具。
游戏入口读取同目录的 game-launch.json。
包内 Runtime、资源与配置依据入口位置解析。
Windows 路径在系统调用边界使用宽字符。

## 公共 UI 与验证入口

公共控件采用即时绘制函数。
sectionHeading、card 与 taskFeedback 统一反馈。
ThemeTokens 声明语义颜色，UiMetrics 声明尺度。
面板按现有注册接口接入工作区。

资源浏览器复用公共搜索、换行工具栏和语义状态。
内容查询与文件事务归既有目录模型和内容服务。
浏览位置、选择和预览状态归编辑器面板。
文件操作继续通过 `EditorSession` 的生产接口。

`ConsoleLogModel` 维护日志快照、级别和暂停状态。
日志面板负责搜索、复制、滚动和任务呈现。
日志模型读取诊断行，也可供离线诊断工具复用。
普通布局使用独立滚动诊断区，紧凑布局使用弹窗。
任务和错误操作共享同一绘制入口与编辑服务。
资源与日志控件的行为见[操作指南](tutorials/browser-console.md)。

项目中心与编辑器可输出只读界面快照。
AZURERENDER_UI_OBSERVATION 指定观察文件。
项目中心输入文件采用既有回放事件格式。
回放修改输入队列，控件调用生产项目服务。

启动验证通过显式环境配置启用。
AZURERENDER_STARTUP_VALIDATION_ENDPOINT 指定端点。
AZURERENDER_STARTUP_VALIDATION_TOKEN 提供会话令牌。
验证通道的认证与操作范围沿用既有服务。
