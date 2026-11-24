# 内容制作与纹理设置

> 文档类型：使用说明与操作参考
> 适用范围：项目资产及编辑器内容浏览器

## 文件操作

在内容浏览器右键点击项目资产。
菜单提供重命名、移动、复制和删除。
按 Ctrl 点击可选择多个资产。
复制依赖树会为所有复制资产创建新 UUID。

1. 选择文件操作，填写名称或虚拟目标路径。
2. 点击 `Preview impact` 检查受影响文件。
3. 保存场景修改后，点击 `Apply` 发布事务。
4. 检查文件、元数据和引用的操作结果。

移动保留 UUID，并维护项目资源引用。
glTF 的外部图片与缓冲区使用合法相对路径。
启动场景移动同步更新项目描述。
导入来源与生成来源随事务进行校验。

`Import / Create` 提供项目目录创建入口。
目录名称使用单个路径段。
项目挂载内的内容支持写入。
`engine:/` 挂载供读取引擎资源。

删除要求所有引用者都在删除集合中。
项目描述所需的启动场景受到保护。
重名、路径越界和只读文件产生明确诊断。
诊断可在通知与控制台中查看。

## 文件撤销与恢复

在 `Import / Create` 点击 `Undo content transaction`。
文件事务使用独立于场景历史的恢复日志。
已提交的文件历史支持项目重开后撤销。
`Redo content transaction` 重新应用文件事务。
撤销前校验输出哈希，保护外部修改。

日志保存于项目的 `.azure/content-transactions/`。
每项事务保存源文件、目标文件及备份哈希。
编辑器打开项目时先恢复中断事务。
备份损坏会产生诊断，供开发者检查日志。

## 纹理设置

在图片资产菜单选择 `Texture settings`。
多选图片可共用同一个设置窗口。
勾选需要应用的字段，再编辑字段值。
通过影响预览确认整批修改。

| 字段 | 合法值与用途 |
| --- | --- |
| schemaVersion | 设置版本为 `1` |
| usage | `albedo`、`emissive`、`normal`、`mask` |
| usage | `roughness`、`linear`、`packed-normal` |
| colorSpace | `srgb` 或 `linear` |
| filter | `linear` 或 `nearest` |
| wrapU / wrapV | `repeat`、`clamp` 或 `mirror` |
| mipPolicy | `generate` 或 `none` |
| anisotropy | `1` 至 `16`，运行时受设备上限约束 |

颜色用途按颜色空间生成纹理层级。
法线用途使用线性空间，并重新归一化。
透明遮罩的过滤考虑最终字节量化与覆盖率。
最近邻过滤使用单倍各向异性。

勾选字段定义本次修改的范围。
未勾选字段保持每个资产的已有配置。
采样字段继承每个材质槽的用途默认值。
用途字段显式指定图片的内容语义。

数据纹理使用线性颜色空间。
设置保存在图片的 `.azmeta` 中。
设置和依赖参与派生产物指纹。
应用成功后更新活动资源与渲染资源。

## 依赖与重导入

资产菜单提供 `Dependencies` 与 `Referenced by`。
`Asset diagnostics` 展示状态、设置与导入来源。
查询结果使用稳定资产身份。
复制依赖树先建立路径与 UUID 映射。
每项来源清单记录转换前的可验证输入。

`Reimport` 创建共同任务服务中的后台任务。
后台读取并校验源文件与依赖。
模型和图片调用实际加载器检查内容。
发布在编辑宿主中完成。

在控制台点击 `Cancel task` 可以取消重导入。
取消保持项目文件和活动资源指纹。
来源指纹或文件变化会使候选结果过期。
重导入期间由会话仲裁制作与预览操作。

## 公共操作

| 操作 | 参数 |
| --- | --- |
| content.create-directory | `parent`、`name` |
| asset.rename | `asset`、`name` |
| asset.move | `asset`、`destination` |
| asset.duplicate | `asset`、`destination`、`includeDependencies` |
| asset.delete | `assets`、`referencePolicy="reject"` |
| texture.settings | `asset` 或 `assets`、`values` |
| texture.describe | `assets` |
| asset.reimport | `asset`、`settings`、`baseFingerprint` |
| asset.dependencies | `asset`、`direction` |
| asset.inspect | `asset` |
| content.undo | 空参数 |
| content.redo | 空参数 |
| content.discard | `planId` |

文件与纹理操作的 `dryRun=true` 返回影响计划。
计划标识通过相同操作的 `planId` 参数应用。
放弃计划使用 `content.discard`。
UI 与自动工具使用相同的操作契约。

场景编辑与对象引用见[属性编辑](property-editing.md)。
项目创建及布局见[项目工作区](project-workspace.md)。
