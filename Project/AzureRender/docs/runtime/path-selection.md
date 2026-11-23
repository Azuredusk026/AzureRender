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
