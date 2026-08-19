# 本机场景展示交付

本目录只保留 2026-08-19 最新的角色与黑洞展示媒体。媒体统一采用“`YYYYMMDD-HHMMSS_简单中文描述`”命名，旧角色交付存放于本地 `archive_legacy/20260819-第六轮/character/`。

- [角色展示与复现](character/CHARACTER_SHOWCASE_CN.md)
- [黑洞展示与复现](blackhole/BLACKHOLE_SHOWCASE_CN.md)

当前角色视频为 1600×900、24 fps、H.264 High、yuv420p、BT.709、SAR 1:1、DAR 16:9；完整解码结果为 1920 帧、80.00 秒。五段分别展示最终渲染、原始模型、法线、阴影和材质分区，每段从朝左开始匀速旋转一周并回到朝左。

眉毛 primitive 已完成拓扑、权重和骨骼审计。它由 34 个眉毛/睫毛卡片小岛组成，权重正常且没有无关骨骼；当前通过 Face D 深红笔画 2 texel UV 膨胀提高可见度，不再缩放网格。

黑洞 P1 已冻结为 Final，不再调整 shader、质量参数、机位或媒体。媒体与私有资产由 `.gitignore` 保护，不进入 Git、CI、安装树或公开作品集。
