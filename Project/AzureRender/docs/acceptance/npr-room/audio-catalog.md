# 原音频目录与资源槽

> 状态为 P2 局部验收。已完成本机媒体与目录对账。

音频入口使用 Recovery Addressables 和五场景的静态依赖。
目录映射包含 1,533 个音频位置及 13 个仅场景引用的来源。
每个来源使用其 Unity GUID 作为稳定资源槽。
Addressables 的键保留其对应资源槽及顺序。

原 OGG 与 WAV 的字节内容随目录发布。
源槽共 1,546 项，去重后为 1,537 份媒体。
源文件合计 530,688,916 字节。
去重媒体合计 526,406,746 字节。

写入阶段校验来源 GUID、容器头和 SHA-256。
媒体复制后逐份复核摘要。
目录完成校验后统一发布。
Windows 临时目录占用按两秒预算重试。

自动检查覆盖两个来源共用同一媒体、键映射及发布占用。
本机全量复制复测退出码为零。
[清单摘要](audio-catalog.json)登记本机目录身份。
原媒体位于 `assets_private/npr-room-dev/audio-catalog`。

## 复验命令

在 AzureRender 根目录运行命令。
先核对规划体积，再在受管任务内写入新目录。

```powershell
python -B tools/npr_room/audio_catalog.py --source $unitySource --inventory $referenceInventory
python -B tools/npr_room/audio_catalog.py --source $unitySource --inventory $referenceInventory --output $newAudioCatalog
```

资源槽按宿主支持的格式接入播放。
完整音频运行验收包括解码、循环点、事件路由和窗景联动。
