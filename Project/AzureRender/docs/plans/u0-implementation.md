# U0 编辑器与游戏界面实施计划

> 文档类型：实施计划
> 状态：Complete
> 更新日期：2026-10-04
> 适用范围：Windows 编辑器与独立 Player

## 目标与执行依据

依据 [开发总计划](azure-engine-plan.md)，完成项目编辑、进程内运行、游戏界面、动画状态机和音频。执行采用自主实施模式，阶段结束创建独立提交。

## 模块与接口

EditorContext 持有编辑场景、组件存档、选择与操作历史。项目入口通过 `openProject(path)` 读取 AssetDatabase 和 Level。导入、放置、复制、删除、组件编辑与保存使用同一份编辑状态。

EditorSession 提供 Play、Pause、Resume、Step 和 Stop 命令。宿主使用独立运行 World，停止恢复编辑态。编辑命令与游戏输入根据焦点和运行状态启用。

`src/runtime/AnimationStateMachine.*` 推进版本化动画状态和条件转换，输出节点的动画播放参数。`src/runtime/AudioRuntime.*` 使用 miniaudio 管理声音、音量、暂停、销毁和离线混音验证。

`src/runtime/GameUi.*` 持有 RmlUi 上下文、资源访问、事件与界面状态。`src/render/GameUiRenderer.*` 使用宿主 RHI 和分配器提交界面几何、纹理与裁剪，进入公共合成流程。

## 文件级任务与验证

- [x] `tests/EditorWorkflowTests.cpp` 验证导入、放置、多选、复制、重命名、删除、撤销与保存重开。
- [x] `tests/GameplayPresentationTests.cpp` 验证动画转换、时间推进、音频混音、暂停与损坏资源处理。
- [x] `tests/GameUiTests.cpp` 验证 RmlUi 文本绑定、交互、资源定位和窗口尺寸变化。
- [x] 完成 EditorContext、EditorSession、ImGuiEditorLayer 和宿主运行控制。
- [x] 集成动画、音频、Lua 接口和公开示例，确认实际渲染、声音及界面输出。
- [x] `tools/test_editor_workflow.py` 在真实 Vulkan 编辑器中执行项目工作流并核验报告。
- [x] Debug 与 Release 完整回归、Validation、界面截图和阶段性能记录通过。
- [x] 同步主题文档、总计划、README、变更记录、许可证和证据，提交 U0。

## 验收与风险

导入失败保留有效场景，运行错误隔离脚本，停止保持编辑内容与操作历史。测试覆盖空场景、多选删除、资产路径、失焦输入、暂停单步、切关和资源释放。

运行时界面由 RmlUi 与 Vulkan 适配层处理。Player 链接运行时和渲染库，编辑器代码进入编辑器目标。音频无设备时提供明确诊断，离线混音用于可重复验证。

完整回归每阶段执行一次，功能开发先运行对应契约测试。GPU 预算沿用现行口径，新增界面与玩法性能另行记录。

## 来源与许可

RmlUi 来源为 https://github.com/mikke89/RmlUi，固定准入版本 6.3，许可证为 MIT。适配参考公开 RenderInterface 与 Vulkan 后端的接口契约，使用项目既有 GPU 所有权。

miniaudio 来源为 https://github.com/mackron/miniaudio，固定准入版本 0.11.25，许可选择 MIT-0。公开字体使用 RmlUi 示例提供的 LatoLatin 与其许可文件。版本、来源文件和适配理由在运行时说明记录。
