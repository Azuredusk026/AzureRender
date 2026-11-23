# 本机路径选择

`AzurePlatform` 提供 Windows 原生路径选择服务。
项目浏览、导入工具和媒体选择可共用该服务。
入口为 `platform/PathSelection.hpp` 中的 `choosePath`。
对话框由调用线程显示，调用在关闭后返回。

## 选择请求

| 字段 | 类型与默认值 | 含义 |
| --- | --- | --- |
| `title` | UTF-8 字符串 | 对话框标题。 |
| `directory` | 布尔值，`false` | 选择目录或文件。 |
| `initialDirectory` | 本机路径，空 | 初始目录。 |
| `extensions` | 字符串数组，空 | 文件显示筛选，例如 `.wav` 和 `.png`。 |
| `owner` | 本机窗口句柄，空 | 对话框所属窗口。 |
| `multiple` | 布尔值，`false` | 允许选择多个项目。 |

文件筛选用于原生对话框展示。
消费方按导入契约校验所选文件的格式和内容。
目录选择保留文件系统目录语义。

## 选择结果

成功时，`paths` 包含全部所选路径。
`path` 等于 `paths` 的首项，供单路径调用使用。
`cancelled` 为 `false`，`diagnostic` 为空。

取消时，`cancelled` 为 `true`，两个路径字段均为空。
失败时，`diagnostic` 保存错误，两个路径字段均为空。
消费方先处理取消和失败，再提交领域操作。

Windows 通过 `FOS_ALLOWMULTISELECT` 声明多选。
结果统一通过 `IFileOpenDialog::GetResults` 提取。
该方法支持单项及多项结果。[微软接口文档](https://learn.microsoft.com/en-us/windows/win32/api/shobjidl_core/nf-shobjidl_core-ifileopendialog-getresults)

## 媒体调用

```cpp
PathSelectionRequest request;
request.title = "选择本地音乐";
request.extensions = {".wav", ".mp3", ".flac", ".ogg", ".m4a"};
request.multiple = true;
const auto selected = choosePath(request);
if (selected.cancelled) return;
if (!selected.diagnostic.empty()) throw std::runtime_error(selected.diagnostic);
for (const auto& file : selected.paths) {
    importSelectedFile(file);
}
```

导入与媒体存档由调用模块持有。
`importSelectedFile` 由消费方实现。
路径服务负责选择结果及取消状态。
编辑器的 `path.choose` 操作使用单路径返回契约。

## 自动验证

`AzurePathSelectionTests` 操作真实 Windows 对话框。
测试建立独立的中文目录和带空格的文件名。
验证单文件、多文件、目录选择和取消。
输入驱动设置实际控件焦点，并触发确认按钮。

```powershell
cmake --build $build --target AzurePathSelectionTests -j 2
& "$build/AzurePathSelectionTests.exe" $fixture
```

## 目录收集

`platform/PathCollection.hpp` 提供 `collectPaths`。
该服务返回只读路径集合，供媒体与资产导入消费。
音乐与图片各自传入扩展名，共用递归和诊断规则。

| 请求字段 | 默认值与范围 | 含义 |
| --- | --- | --- |
| `inputs` | 路径数组 | 显式选中的文件或目录。 |
| `extensions` | 空数组 | 点号开头的扩展名；大小写不敏感。空数组收集全部普通文件。 |
| `recursive` | `false` | 是否递归所选目录。 |
| `maxFiles` | 4,096；1–100,000 | 去重后的文件数预算。 |
| `maxEntries` | 16,384；1–1,000,000 | 已检查的输入和目录条目预算。 |
| `maxDepth` | 32；1–128 | 目录深度预算，所选目录为零层。 |
| `cancelled` | 空回调 | 合作式取消查询。 |

`files` 返回规范路径，并按路径稳定排序。
同一物理文件、硬链接和重复目录各处理一次。
递归跳过符号链接和 Windows 重解析子目录。
显式输入可选择链接目标。

`issues` 保存对应路径和诊断，可与可用文件并存。

取消及预算耗尽时，文件列表为空。
对应的 `cancelled` 或 `budgetExceeded` 状态为真。
空选择也检查预先取消状态。
无效预算或扩展名配置抛出 `std::invalid_argument`。

`AzurePathCollectionTests` 使用八种媒体扩展名。
检查覆盖中文目录、硬链接、循环目录链接及三类预算。
循环链接在测试退出时释放，目标目录及文件保留。
链接释放采用[Windows 非递归接口](https://learn.microsoft.com/en-us/windows/win32/api/fileapi/nf-fileapi-removedirectoryw)。

```powershell
cmake --build $build --target AzurePathCollectionTests -j 2
& "$build/AzurePathCollectionTests.exe" $fixture
```
