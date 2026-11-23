# 运行时媒体任务

`LocalMediaService` 归属应用装配模块。
构造时接收共享项目存档和本机选择器。
平台服务负责路径选择和有界收集。
媒体库负责解码、项目副本及清单事务。

## 选择与导入

`select(kind, directory)` 打开本机多选对话框。
`kind` 为 `music` 或 `images`。
音乐筛选 WAV、MP3、FLAC、OGG 和 M4A。
图片筛选 PNG、JPG 和 JPEG。

`directory` 选择目录并递归收集允许的文件。
文件选择使用完整多选结果。
原生取消返回 `false`，状态为 `cancelled`。
调用方在拥有界面的线程执行选择。

`startPaths` 供可信应用入口提交已选择路径。
单次输入至多 4,096 项。
收集及逐文件导入在一个自有工作线程执行。
任务完成并被查询后，可启动下一项任务。

损坏文件记录逐项失败，合法文件继续导入。
取消保留已经提交的条目。
服务析构时请求取消，并等待自有任务结束。
共享存档在工作线程使用期间保持有效。

## 状态与条目

`status()` 返回 `[state, completed, total, failed, message]`。
状态为 `idle`、`running`、`completed`、`cancelled` 或 `failed`。
逐文件失败记录于 `failed` 和诊断文本。
`completed` 表示已处理文件数，包含逐项失败。

`entries(kind)` 每行返回 `[id, title, reference, available]`。
音乐和图片分别使用 `local-media-music` 与 `local-media-images`。
条目可用性沿用[媒体库](media-library.md)的检查范围。
界面通过状态与条目表达结果。

## 资产解析

引用格式为 `media:/<kind>/<id>`。
`resolve` 只解析已提交条目，其他命名空间返回空结果。
应用将解析器注册到 `AssetDatabase::setReferenceResolver`。
数据库副本及关卡后台准备保留解析器。

解析器由可信宿主持有，返回只读文件定位。
音频组件与图片加载沿公共资产解析接口消费引用。
媒体缓存通过条目标识定位，保持源目录的独立所有权。
常规项目资源继续使用所属项目的引用规则。

## 脚本与图片

宿主通过 `setMediaService` 注入 `IMediaLibraryService`。
脚本调用 `media_import`、`media_entries` 和 `media_status`。
`media_cancel` 请求取消当前工作。
导入与取消需要运行中的脚本回调。

`ui_image(id, reference)` 设置 RML 的 `img` 元素。
空引用清除图片，合法引用经资产数据库解析。
界面路径转换与原生解码共同支持中文和深目录。
初始化事务可暂存图片设置。

本批运行配置为 Windows Release 与 Lua。
托管桥接和 C# 生成物按同一契约同步。
[自动证据](../acceptance/npr-room/platform-scenarios.md#运行时媒体任务与定位)记录实际执行范围。
