# 资产交换与后台导出

格式注册声明写入器、规划器和设置校验器。
扩展通过 `AssetFormatRegistry::add` 接入。
任务持有注册表副本及来源快照。
编辑器与独立工具使用相同的传输契约。

## 输出范围

| 领域 | 格式 | 保真与损失 |
| --- | --- | --- |
| 模型 | glTF、GLB | 几何、材质、骨架及受支持动画 |
| 模型 | OBJ | 静态几何、材质及独立 PNG 贴图 |
| 模型 | STL、PLY | 静态几何；其他字段由损失报告声明 |
| 图像 | PNG、TGA、WebP | 八位 RGBA；PNG 支持十六位样本 |
| 图像 | BMP、JPEG | 整数颜色；JPEG 使用固定质量设置 |
| 图像 | HDR | 非负线性 RGB，采用共享指数精度 |
| 图像 | EXR、TIFF | 线性浮点范围及透明通道 |
| 音频 | WAV | 交错 Float32 样本 |
| 音频 | FLAC | 二十四位整数 PCM，声明量化及裁剪 |
| 音频 | Ogg Vorbis | 有损编码，记录质量及采样转换 |

项目脚本、物理及任务配置保存在项目格式中。
模型输入与贴图共用有界读取服务。
容器拥有贴图，OBJ 输出拥有材质及 PNG 依赖。
场景子树描述节点标识、父子关系和局部变换。

## 规划与发布

`planAssetExport` 校验来源、设置和版本。
规划返回入口、依赖、来源指纹和转换损失。
规划阶段保持输出目录尚未创建。
生成通过 `AssetExportJob` 在后台准备。

任务保存不可变子树描述，并核对来源指纹。
调用线程在任务就绪后执行 `finish`。
目标已占用时，需要显式允许覆盖。
覆盖采用目录备份，失败或取消时恢复原目标。

取消与析构清理任务拥有的暂存目录。
生成记录输入、输出、参数及生成器版本。
模型输出预算为 512 MiB。
图像及音频输出预算分别为 64 MiB 和 128 MiB。

## 独立工具

安装中的 `bin/AzureAssetExchange.exe` 执行交换。
请求文件使用 UTF-8 JSON。
相对路径以请求文件所在目录为基准。
结果以 JSON 输出，失败返回退出码一。

```json
{
  "source": "assets/模型.glb",
  "format": "gltf",
  "destination": "exchange/模型",
  "settingsVersion": 1,
  "settings": {}
}
```

```powershell
AzureAssetExchange.exe plan request.json
AzureAssetExchange.exe export request.json --deadline-ms 300000
AzureAssetExchange.exe export request.json --replace
```

默认期限为五分钟，最长允许一小时。
Ctrl+C 请求取消当前生成任务。
设置由目标格式校验，未知字段产生错误。
图像质量取一至一百，默认值为一百。

音频设置包含 `sampleRate` 和 `channels`。
零表示采用来源值。
采样率范围为 8000 至 192000，声道为一至八。
Vorbis 质量范围为零至一，默认值为 0.6。

`scene` 数组可提供模型节点描述。
每个节点具有 `id`、`parent`、`source` 和 `transform`。
变换采用列主序的十六个有限数值。
空来源表示组节点，节点总数至多 4096。

重复标识、缺失父节点及循环关系产生诊断。
静态格式烘焙父子变换，容器保留节点标识。
正式交互通过 U17 验收，实体复核集中在 P8。
