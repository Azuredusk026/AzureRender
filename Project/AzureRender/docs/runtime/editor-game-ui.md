# 项目编辑与游戏表现

> 文档类型：运行时说明
> 状态：生效
> 更新日期：2026-10-04
> 适用范围：Windows 编辑器与 Player
> 源码入口：`src/editor/`、`src/runtime/PresentationRuntime.*`、`src/render/GameUiRenderer.*`

## 打开与编辑项目

使用 `AzureRender.exe --editor-project <project.azureproject>` 打开项目。公开示例位于 `assets_public/gameplay/project.azureproject`。基础 `.azscene` 项目与 JSON 关卡均可编辑。

资源浏览器接受 glTF 或 GLB 文件路径。导入任务复制模型与相对依赖，验证候选资源后提交。进度与取消按钮显示当前状态。取消和失败保留有效场景。

双击资源可放置对象，也可将资源拖入视口。层级中按 Ctrl 可多选。复制、删除、重命名和组件字段编辑进入操作历史。Ctrl+D 复制，Delete 删除，Ctrl+Z 撤销，Ctrl+Y 重做。

Inspector 根据反射字段提供角色、刚体、脚本、动画、音频和界面属性。字段类型和数值范围在提交时校验。Ctrl+S 保存项目关卡。渲染参数、组件、原生节点和 Prefab 实例覆盖随关卡保存。

## 运行与输入

Run 菜单与主菜单工具栏提供 Play、Pause、Resume、Step 和 Stop。Ctrl+P 启动或停止运行。Play 从当前编辑内容创建独立 World。暂停时 Step 推进一次 60 Hz 固定步。

运行时组件和关卡使用运行 World。停止后显示编辑内容，编辑历史继续可用。运行期间编辑面板按启用条件锁定。

视口获得焦点时接收 WASD、Space、E 与相机鼠标输入。Esc 释放视口输入与鼠标捕获，点击视口获取焦点。文本与交互控件由界面焦点处理。

Console 显示脚本、关卡和表现资源的错误。修改 Lua 文件后进行受控重载。失败重载保留有效脚本，运行错误隔离对应回调。Player 使用 P 暂停或恢复，O 推进一次暂停步。

## 组件与接口

| 组件 | 字段与行为 |
| --- | --- |
| `azure.animator` | 状态图、实体时钟、淡化、速度驱动与 Morph，使用版本 2 |
| `azure.third-person-camera` | 跟随目标、距离、俯仰、肩偏移与遮挡参数，使用版本 1 |
| `azure.audio-source` | `asset`、`loop`、`autoplay`、`volume` 和 `enabled` 控制声音 |
| `azure.game-ui` | `asset` 指向 RML 文档，`enabled` 控制界面 |

动画图版本为 1，包含 `initial`、`states` 和 `transitions`。状态指定 glTF 动画索引。条件转换读取布尔参数，切换状态重置播放时间。公共示例的 idle 与 run 使用两种动画节奏。

角色渲染器按节点身份采样各资源的独立姿态。相同资源共享骨架、片段和原始网格，各实例保留自己的时钟、混合、头部基底与 Morph。配置与验收入口见[独立动画与第三人称控制](third-person-animation.md)。

miniaudio 提供设备输出和离线混音。声音随实体、组件和关卡生命周期释放。暂停保留播放游标，恢复继续播放。无设备环境记录诊断，并使用离线混音。

Lua 的 `self:audio_play()` 播放当前实体音源，`self:ui_text(id, text)` 更新界面纯文本。这两个接口遵守回调和初始化事务。失败初始化的副作用保持隔离。

## 游戏界面

RmlUi 负责文档布局、字体、文本与事件。GameUiRenderer 通过项目 RHI 创建几何、纹理和管线，并在公共后处理合成阶段绘制。Player 的链接边界为运行时与渲染模块。

适配层支持纹理、字体、预乘透明、矩形裁剪和矩阵变换。文档使用这些能力构建界面。窗口尺寸与显示密度传给 RmlUi，编辑器将鼠标坐标转换到视口像素。

GPU 对象按在途帧延迟回收。交换链重建和退出在 GPU 空闲后销毁界面资源。高级裁剪蒙版、滤镜和图层属于后续适配能力。

## 自动验收

`--editor-actions <json>` 执行按帧排序的生产编辑命令。`--runtime-report <json>` 输出操作耗时、运行步数、切关、错误恢复、动画帧与界面绘制统计。

```powershell
python tools/test_editor_workflow.py --executable build/ninja-msvc-debug/AzureRender.exe --project assets_public/gameplay/project.azureproject
python tools/test_game_presentation.py --executable build/ninja-msvc-debug/AzurePlayer.exe --project assets_public/gameplay/project.azureproject
```

编辑任务验证导入、编辑、历史、保存、运行、脚本修复、切关和停止。图像任务比较实际状态动画与界面合成。单元验证覆盖导入取消、Prefab、基础项目兼容、混音与资源退休。

`build` 命令接收 `install`、`output` 和可选 `replace`。`wait-build` 按帧等待异步构建，并检查发布结果。自动化任务完成后结束烟雾运行，完整流程见 [Windows 游戏发布](game-publishing.md)。

## 来源与许可

[RmlUi 6.3](https://github.com/mikke89/RmlUi/tree/6.3) 使用 MIT 许可。[miniaudio 0.11.25](https://github.com/mackron/miniaudio/tree/0.11.25) 采用 MIT-0 许可。字体来自 RmlUi 6.3 示例，LatoLatin 使用 SIL OFL 1.1。

本地 Vulkan 适配依据公开 RenderInterface 实现。资源由宿主分配器和在途帧管理。公开音效由项目生成，模型与动画由项目公开测试资产派生。许可证进入安装清单。
