# 编辑器接入回执

核验日期为 2026 年 11 月 19 日。
主线基点为 `dbe4ab3f84f55ff6776634842c4ac794cf887345`。
交付候选为 `5d4cf7dbf92b9a05181a7e11a2ff81b4cca7f3b3`，包含 `e729305`。
目标树为 `271036aaffed05620a57eebf11cd25295d5385a9`。

接入补丁从封存证据包逐字节恢复。
SHA-256 为 `eff0b3168fb28177fb5451c24bc25911e863993fe57d0cfc86311d648b4f7770`。
隔离 Git 索引从主线基点加载，应用补丁后写出的树与目标树一致。 [补丁](dispatch-1497/editor-5d4cf7d-on-dbe4ab3.patch) · [机器回执](integration-receipt.json)

## 文件范围

两侧已提交改动的交叉文件集合为空。
目标树与候选树的差异仅为主线联合场景验收文档。
构建输入一致性用于确定历史验收的复用范围。
接入时以提交和 Git 对象为输入，工作区中的共享目录联接及本地改动保留各自归属。

| 接入文件 | 行为与复验范围 |
| --- | --- |
| `SelectionService.cpp/.hpp` | 显式节点定位请求层级焦点，普通选择保持既有路由。核对选择、引用与撤销历史。 |
| `commands/EditorOperations.cpp` | 节点引用定位调用选择服务。核对属性面板和服务入口。 |
| `ImGuiEditorLayer.cpp` | 定位时打开层级并安排窗口焦点。核对关闭面板后的定位。 |
| `panels/OutlinerPanel.cpp`、`CorePanels.hpp` | 展开场景和父级路径，保存并恢复同文档搜索。核对创建、定位、返回及替换搜索。 |
| `Catalog.inc` | 搜索恢复入口的中英文词条。 |
| 两份回放工具、变更记录和证据索引 | 既有验收操作与交付说明。 |

共享渲染宿主的后台准备属于已接入的 `803e42a`。
本次补丁使用上述编辑器入口。

## 可访问证据

每份日志副本与原封存清单中的长度和 SHA-256 核对。
两份精简包分别保留 288 和 204 份可验证文件。
包内缺项清单见机器回执的 `omittedFiles`。

| 历史批次 | 保留的检查 | 当前副本 |
| --- | --- | --- |
| 1475 | Release 的节点添加、选择服务、编辑会话和创建共 4 项 CPU 检查；探索与资产检视选择回放；两项目各两档缩放的创建回放 | [CPU 日志](dispatch-1475/release-ctest.log) · [两项目选择](dispatch-1475/candidate-selection/run-1794816865490986100/summary.json) · [创建回放](dispatch-1475/candidate-creation/run-24d3754a/summary.json) |
| 1497 | Release 的选择服务、编辑会话和属性编辑共 3 项 CPU 检查；两项目引用定位和搜索恢复；引用选取、取消、资源定位和保存重开 | [CPU 日志](dispatch-1497/release-ctest.log) · [属性回放](dispatch-1497/properties/summary.json) · [两项目选择](dispatch-1497/selection/run-1794821403043037300/summary.json) |

两批进程回执均记录退出码为零。
批次编号关联来自候选提交中的证据索引。
当前活动队列库中已无这两条原始行。
原运行目录和受检程序已回收，程序摘要按提交内索引引用。
本次执行 Git 树、补丁和归档字节核验，功能结果来自历史回放。 [批次 1475 回执](dispatch-1475/managed-process-receipt.json) · [批次 1497 回执](dispatch-1497/managed-process-receipt.json) · [原索引副本](source-index.json)

## 接入与待验

性能与集成线持有主线合入及必要复测。
按指定基点接入且构建输入相同时，可复用本表历史结果。
新增源码、配置、资产或运行环境差异时，接入负责人核对其影响并执行相应交叉路径。
实体键鼠、输入法和多屏继续归属现场复核。

## 独立核验

执行下列命令可在隔离索引重新计算目标树。

```powershell
git merge-tree --write-tree dbe4ab3 5d4cf7d
```

本目录的脚本读取原封存清单、精简证据包和进程回执，逐项核对后重建机器回执及小型日志副本。
脚本所需原包地址和摘要均在机器回执中列出。 [核验脚本](rebuild.py)
