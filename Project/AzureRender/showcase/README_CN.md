# 本机场景展示交付

本目录只保留最新的角色与黑洞展示媒体。媒体统一采用“`YYYYMMDD-HHMMSS_简单中文描述`”命名；上一轮 1600×900 角色交付存放于本地 `archive_legacy/20260820-第七轮/character/`。

- [角色展示与复现](character/CHARACTER_SHOWCASE_CN.md)
- [黑洞展示与复现](blackhole/BLACKHOLE_SHOWCASE_CN.md)

当前角色重点交付新增两段独立视频：Face SDF 最终渲染和 PCSS 阴影可见度。两段均为原生 2560×1440、24 fps、H.264 High、yuv420p、BT.709、SAR 1:1、DAR 16:9；每段完整解码 384 帧、16.00 秒，从朝左开始匀速旋转一周并回到朝左。原 80 秒五模式视频保留为综合技术展示。

最终动画 GLB 现已嵌入 1024x1024 Face SDF，并绑定唯一的 `Bip001_Head`；运行时不再使用 2x2 回退纹理。眉毛 primitive 已完成拓扑、权重和骨骼审计。它由 34 个眉毛/睫毛卡片小岛组成，权重正常且没有无关骨骼；当前通过 Face D 深红笔画 2 texel UV 膨胀提高可见度，不再缩放网格。

黑洞 P1 已冻结为 Final，不再调整 shader、质量参数、机位或媒体。媒体与私有资产由 `.gitignore` 保护，不进入 Git、CI、安装树或公开作品集。
