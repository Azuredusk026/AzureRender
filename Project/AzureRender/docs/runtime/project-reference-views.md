# 项目参考视图配置

项目将具名参考视图保存到 `config/reference-views.json`。
`ProjectReferenceViews` 持有版本一集合及数值副本。
运行装配与校准工具通过公开读写接口消费该集合。
配置的场景选择与绘制绑定由所属宿主装配。

## 文件格式

根字段为 `schemaVersion` 和 `views`。
`views` 将稳定名称映射到参考相机视图。
文件至多 1 MiB，集合至多六十四项。
名称由一至六十四个 ASCII 字母、数字、点、横线或下划线构成。

每个视图声明版本、坐标帧、投影、尺寸、深度约定及纵轴方向。
坐标帧使用三个有符号轴索引及三个偏移量。
投影使用四个四维列及二维归一化抖动。
深度约定使用 `forward` 或 `reversed`。

`produceSceneDepth` 为可选布尔字段，缺省为 `false`。
启用时，视图声明反向深度及单样本空间绘制。
保存时以 `true` 表示启用。
附件说明见[场景参考深度](scene-reference-depth.md)。

`produceSceneVisibilityInputs` 为可选布尔字段。
启用时，视图同时声明 `produceSceneDepth`。
附件包含压缩法线及对象身份。
保存时以 `true` 表示启用。

`objectVisibility` 为可选对象，要求可见量输入启用。
对象包含 `pixelOffset`，每个分量介于负一和一。
该偏移在参考附件的整数像素坐标上应用。
场景对象可见量的输出格式见[场景参考深度](scene-reference-depth.md)。

`objectVisibility.calibration` 可声明原始逆投影标定。
对象包含版本一、`positionOffset` 和 `clipToWorld`。
`positionOffset` 使用三个有限的参考坐标原点分量。
`clipToWorld` 使用列主序的十六个单精度分量。

`calibration.objectRecords` 可配置至多八个对象记录。
每条记录的 `atlasPage` 为零至七，页号在视图内唯一。
`anchorWorldToUnit` 保存锚点场景的世界到正向单位投影。
`referenceToUnit` 保存标定坐标到反向单位投影。

两个对象矩阵均使用十六个列主序单精度分量。
矩阵须有限、可逆并满足仿射格式。
`lightDirection` 为标定坐标中的三分量单位向量。
图集分区、过滤半径与偏置取自当前场景。

标定矩阵须有限且可逆。
标定姿态使用保存的矩阵位值。
相机移动和屏幕缩放同步搬移标定。
对象记录沿用同一参考坐标轴和原点。

嵌套坐标帧及投影各声明版本一。
保留字段采用所属数学类型的零值。
未知字段、版本及非法数值按输入错误处理。
所有矩阵、尺寸及数值范围共用参考视图的准备校验。

## 读写与所有权

`loadProjectReferenceViews` 接收已加载的项目。
配置路径相对于项目文件所在目录。
配置缺失时返回空集合，其他读取失败提供异常。
返回集合独立持有数值，项目移动后按新路径重开。

`saveProjectReferenceViews` 接收完整集合。
保存先校验并生成有界文本，再写入同目录临时文件。
文件刷新和关闭后，通过重命名替换目标。
失败时清除所属临时文件，并保留目标内容。

调用方串行管理同一项目的配置写入。
JSON 编解码归属 RenderCore，项目文件读写归属 Runtime。
CPU 数值副本及 GPU 资源各由消费方持有。
专项结果见[项目参考视图验收](../acceptance/nr/project-reference-views.md)。
