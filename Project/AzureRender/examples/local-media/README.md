# 本地媒体工作间

本示例使用公共 Player、脚本接口和项目媒体服务。
音乐与图片通过原生对话框导入。
已提交副本保存在指定项目档案内。
界面使用 1280×720 像素工作区。

## 生成与启动

在引擎根目录执行命令。
将 `$player` 指向已安装的 `AzurePlayer.exe`。
将 `$resources` 指向同版本安装资源根目录。
资源目录包含编译着色器、默认字体及中文回退字体。

```powershell
$mediaRun = python -B tools/artifact_cleanup.py --new-run local-media-example --planned-bytes 16777216
python -B tools/build_local_media_example.py --output "$mediaRun/workspace"
& $player --project "$mediaRun/workspace/project.azureproject" --save-root "$mediaRun/profile" --resource-root $resources --width 1280 --height 720
python -B tools/artifact_cleanup.py --finish $mediaRun
```

输出工程目录须由本次生成独占。
项目身份保持固定，同一档案根目录可恢复已导入条目。
`--save-root` 为本次运行提供持久项目档案。
产物保留按[项目产物规则](../../docs/development/artifact-retention.md)执行。

## 操作

音乐区支持文件多选和文件夹递归导入。
支持格式为 WAV、MP3、FLAC、OGG 和 M4A。
上一首、播放、暂停及下一首使用同一音频组件。
曲目结束后按列表继续播放。

图片区支持 PNG、JPG 和 JPEG。
同一内容复用条目，上下张切换保留当前选择。
源文件搬移后，项目继续使用已导入副本。
缺失缓存可通过重新导入源文件恢复。

取消操作保留已经完成的导入。
损坏文件记录失败，其余合法文件继续处理。
列表、失败状态和重开使用[运行时媒体契约](../../docs/runtime/local-media-service.md)。
实体操作和听感按[人工复验登记](../../docs/acceptance/npr-room/manual-retest.md)记录。
