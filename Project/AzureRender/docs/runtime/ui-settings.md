# UI 基础、面板与设置

> 文档类型：运行时与编辑器契约
> 状态：生效
> 更新日期：2026-10-06
> 适用范围：Windows 编辑器与 Player

## 职责与入口

UI 基础位于 `src/editor/ui/`。
语义颜色区分文本、表面、边框和操作状态。
尺度由 `UiMetrics::fromScale()` 统一计算。
有效缩放范围为 0.75 至 3。

`Widgets` 提供按钮与属性行。
按钮声明变体、选中、禁用和提示。
作用域对象负责样式、ID 和禁用栈。
菜单、工具栏、属性与状态栏使用同一基础。

`src/editor/panels/` 保存各面板实现。
工作区拥有开关、停靠和控件状态。
默认显示九个制作与诊断面板。
Settings 面板通过 View 或 Tools 打开。

## 扩展面板与选择

应用通过 `EditorSession::panelRegistry()` 注册面板。
工厂提供稳定标识、版本与能力描述。
注册应在创建 `ImGuiEditorLayer` 前完成。
宿主将面板加入工作区和持久化布局。

`IEditorPanel::draw()` 接收 `PanelContext`。
它提供只读文档、选择和编辑服务。
只读视图提供当前资产服务与领域数据。
文档修改通过 `EditService` 执行。

`SelectionService` 使用节点标识。
整组选中对象先校验，再经生产操作提交。
删除、撤销和重开使用当前文档解析选择。
未知或重复标识得到明确拒绝。

面板标识由字母、数字、点和短横线组成。
下划线也属于有效字符。
标题使用普通文字，工作区负责稳定窗口后缀。
面板总数上限为 64。

布局文件为版本 2。
标准面板和已注册扩展均参与保存。
可选面板使用注册声明作为缺失项默认值。
损坏布局按工作区声明恢复。

## 类型化设置

`SettingRegistry` 属于 Foundation。
注册值支持布尔、整数、有限数值和字符串。
声明包含默认值、范围、描述和访问标记。
读写由所属宿主线程执行。

只读标记限定声明拥有者。
启动专用值在 `start()` 后保持固定。
持久化标记决定用户文件保存范围。
每次注册同时校验默认值和范围。

来源按下表从低到高覆盖。
每层独立保存，可以恢复低层有效值。
`describe()` 返回类型、来源和待应用值。
注册总数上限为 1024。

| 顺序 | 来源 | 归属 |
| --- | --- | --- |
| 1 | Default | 模块声明 |
| 2 | DefaultFile | 引擎默认文件 |
| 3 | UserFile | 用户偏好 |
| 4 | Project | 项目设置文件 |
| 5 | CommandLine | 当前启动参数 |
| 6 | Console | 当前会话覆盖 |

`set()` 与 `reset()` 修改候选层。
`replaceLayer()` 完整校验目标层。
帧边界调用 `applyPending()` 生效。
批量失败保持候选层与已生效值。

用户文件使用整数 `schemaVersion: 1`。
`values` 为名称到标量值的映射。
文件预算为 2 MiB，单个字符串预算为 64 KiB。
缺失文件采用模块与其他来源的有效值。

损坏用户文件记录诊断并保持有效层。
默认文件和项目文件错误使启动失败。
未知字段、非法类型和越界值均拒绝。
`saveUser()` 保存允许持久化的用户层。

```json
{
  "schemaVersion": 1,
  "values": {
    "editor.scale": 1.0,
    "input.cameraSensitivity": 1.0,
    "render.exposure": 0.5
  }
}
```

## 模块声明与生效快照

Render Core 声明渲染设置的类型与范围。
Runtime 装配渲染、诊断和输入偏好。
Editor 声明尺度与紧凑布局偏好。
Player 使用同一基础注册表。

| 名称 | 使用位置 |
| --- | --- |
| `render.diagnosticView` | 生效渲染快照与观察查询 |
| `render.exposure` | 生效渲染快照中的曝光 |
| `render.blackholeQuality` | 黑洞渲染的质量选择 |
| `diagnostics.verbose` | 设置提交后的来源诊断 |
| `input.cameraSensitivity` | 窗口鼠标与编辑视口相机 |
| `editor.scale` | 系统 DPI 的缩放倍率 |
| `editor.compact` | 编辑器紧凑工作区 |

关卡文件拥有制作态渲染设置。
每帧从制作态生成独立生效快照。
显式设置来源覆盖该快照的对应字段。
仅有声明默认值时，快照采用关卡字段。

设置修改保持文档内容与历史的版本。
重置显式覆盖后，快照读取当前关卡值。
`render.exposure` 查询报告实际渲染值。
`setting.render.exposure` 报告注册表值。

玩法动作及绑定归属于项目运行配置。
输入偏好控制物理鼠标的相机灵敏度。
项目系统仍通过 G8 的动作配置组合。
配置定义见[系统组合](system-composition.md)。

## 编辑与自动操作

设置面板和自动入口使用相同生产操作。
`settings.set` 接收名称、值和写入来源。
`settings.reset` 清除指定来源。
`settings.describe` 提供搜索与来源描述。

编辑写入来源支持 `console` 和 `user-file`。
设置操作拥有独立的会话副作用。
操作结果标记为待帧边界应用。
文档事务按其操作描述校验组合范围。

Save user preferences 保存用户层。
保存请求在下一帧边界执行。
默认路径为编辑器配置目录的 `settings.json`。
`--settings-file` 指定本次用户文件。

观察名称由设置注册描述生成。
`setting.<名称>` 返回已生效值。
`setting.source.<名称>` 返回获胜来源。
验证协议见[观察与内容来源](observations-generation.md)。

```powershell
& "D:/AzureEngine/bin/AzureRender.exe" `
  --editor-project "D:/Games/Project/project.azureproject" `
  --settings-file "D:/Preferences/settings.json" `
  --set "render.exposure=1.25"
```

默认与项目文件入口分别为 `--default-settings` 和 `--project-settings`。
传入的文件使用相同设置模式。
这些路径属于当前宿主启动配置。
工具包装将宿主参数完整转发。

## 验证与来源

专项测试覆盖来源、帧边界和失败恢复。
独立面板夹具验证注册与稳定选择。
两项目实测覆盖设置控件、保存和重开。
工作区回放覆盖文字焦点、拾取和停靠。

```powershell
ctest --test-dir build/ninja-msvc-debug `
  -R "^Azure(Editor.UiFoundation|Editor.PanelContext|Editor.SettingsHost|Engine.SettingRegistry|Engine.SettingsOverlay)$" `
  --output-on-failure
```

语义样式、面板和来源借鉴 B1、B2、B3。
参考提交为 `4ba5b7cd106c282e7ed166ff853aeea87b392680`。
作用域适配保留 gameknife 的 MIT 版权和许可。
版权记录位于 `THIRD_PARTY_NOTICES.md`。
借鉴依据见[架构评估](../research/2026-10-06-gknextengine.md)。
