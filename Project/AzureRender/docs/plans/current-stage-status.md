# 当前阶段状态

项目状态：开发中。

本页回答各阶段处于什么状态，下一项工作是什么。
阶段状态取值为完成、Ready、开发中、待完成。
完成表示本阶段规定的交付和检查已满足。
Ready 表示依赖满足，可以开始执行。
开发中表示当前正在执行，待完成表示尚未具备启动条件。

机器清单为相邻的 `stage-status.json`。
本页由 `tools/check_stage_status.py --write-page` 生成。

## 阶段清单

| 代号 | 工作 | 状态 | 范围与依据 |
| --- | --- | --- | --- |
| F4 | 模块与类型契约 | 完成 | 已验收范围及原型边界。[记录](../acceptance/f4/2026-10-06.md) |
| G8 | 共享机制与应用分层 | 完成 | 已验收范围及原型边界。[记录](../acceptance/g8/2026-10-06.md) |
| U3 | 编辑操作与文档事务 | 完成 | 已验收范围及原型边界。[记录](../acceptance/u3/2026-10-06.md) |
| F5 | 观察、内容与工具闭环 | 完成 | 已验收范围及原型边界。[记录](../acceptance/f5/2026-10-06.md) |
| U4 | UI 基础与设置 | 完成 | 已验收范围及原型边界。[记录](../acceptance/u4/2026-10-06.md) |
| G9 | 有界 AI 内容辅助 | 完成 | 已验收范围及原型边界。[记录](../acceptance/g9/2026-10-06.md) |
| R7 | 热重载与资产预览 | 完成 | 已验收范围及原型边界。[记录](../acceptance/r7/2026-10-06.md) |
| G10 | 程序化内容 | 完成 | 已验收范围及原型边界。[记录](../acceptance/g10/2026-10-07.md) |
| R8 | 着色模块原型 | 完成 | 已验收范围及原型边界。[记录](../acceptance/r8/2026-10-07.md) |
| R9 | GPU 可见性原型 | 完成 | 已验收范围及原型边界。[记录](../acceptance/r9/2026-10-07.md) |
| G11 | 脚本后端原型 | 完成 | 已验收范围及原型边界。[记录](../acceptance/g11/2026-10-07.md) |
| P3 | 引擎复用与交付 | 完成 | 已验收范围及原型边界。[记录](../acceptance/p3/2026-10-07.md) |
| U5 | 导航与快捷键 | 完成 | 本阶段交付。[记录](../acceptance/u5/2026-10-08.md) |
| U6 | 选择与变换 | 完成 | 本阶段交付。[记录](../acceptance/u6/2026-10-08.md) |
| U7 | 内容制作与恢复 | 完成 | 本阶段交付。[记录](../acceptance/u7/2026-10-08.md) |
| P4 | 编辑器体验收尾 | Ready | 本阶段交付。[记录](editor-usability-manifest.json) |
| U8 | 工作区与配置 | 完成 | 本阶段交付。[记录](../acceptance/u8/2026-10-10.md) |
| U9 | 属性与引用 | 完成 | 本阶段交付。[记录](../acceptance/u9/2026-10-10.md) |
| G12 | 资产文件操作 | 完成 | 本阶段交付。[记录](../acceptance/g12/2026-10-10.md) |
| U10 | 资源导航与打开 | 完成 | 本阶段交付。[记录](../acceptance/u10/2026-10-10.md) |
| U11 | 日志与诊断 | 完成 | 本阶段交付。[记录](../acceptance/u11/2026-10-10.md) |
| U12 | 运行预览会话 | 完成 | 本阶段交付。[记录](../acceptance/u12/2026-10-10.md) |
| G13 | 制作与运行机制 | 完成 | 本阶段交付。[记录](../acceptance/g13/2026-10-10.md) |
| U13 | 预览与交互反馈 | 完成 | 本阶段交付。[记录](../acceptance/u13/2026-10-10.md) |
| G15 | 探索路线与环境 | 完成 | 本阶段交付。[记录](../acceptance/g15/2026-10-10.md) |
| G16 | 平台玩法与恢复 | 完成 | 本阶段交付。[记录](../acceptance/g16/2026-10-10.md) |
| P5 | 性能稳定性与交付收尾 | Ready | 本阶段交付。[记录](2026-10-12-p5-deferred-to-p8.md) |
| U15 | 制作效率基线 | Ready | 自动门禁通过，集中收尾待验。[记录](../acceptance/u15/automatic-2026-10-12/manifest.json) |
| U16 | 制作缺陷与恢复 | Ready | 自动门禁通过，集中收尾待验。[记录](../acceptance/u16/automatic-2026-10-12/manifest.json) |
| P6 | 双工作流制作交付 | Ready | 自动门禁通过，集中收尾待验。[记录](../acceptance/p6/2026-10-12.md) |
| G17 | 格式注册与交换基础 | Ready | 自动门禁通过，最终复核归属 P8。[记录](../acceptance/g17/evidence/2026-10-12/manifest.json) |
| G18 | 模型与场景交换 | Ready | 自动门禁通过，最终复核归属 P8。[记录](../acceptance/g18/evidence/2026-10-12/manifest.json) |
| G19 | 图像格式交换 | Ready | 自动门禁通过，最终复核归属 P8。[记录](../acceptance/g19/evidence/2026-10-12/manifest.json) |
| G20 | 第一批音频交换 | Ready | 自动门禁通过，最终复核归属 P8。[记录](../acceptance/g20/evidence/2026-10-12/manifest.json) |
| G21 | 场景数据交换 | Ready | 自动门禁通过，最终复核归属 P8。[记录](../acceptance/g21/evidence/2026-10-12/manifest.json) |
| U17 | 导入导出交互 | Ready | 自动门禁通过，最终复核归属 P8。[记录](../acceptance/u17/evidence/2026-10-12/manifest.json) |
| P7 | 第一批格式集成交付 | Ready | 自动门禁通过，最终复核归属 P8。[记录](../acceptance/p7/evidence/2026-10-12/manifest.json) |
| G22 | 压缩纹理与 mip | Ready | 自动门禁通过，最终复核归属 P8。[记录](../acceptance/g22/evidence/2026-10-12/manifest.json) |
| G23 | 压缩音频与编码 | Ready | 自动门禁通过，最终复核归属 P8。[记录](../acceptance/g23/evidence/2026-10-12/manifest.json) |
| U14 | 材质画布 | 完成 | 专项规格规定的实现与检查。[记录](../acceptance/u14/2026-10-12.md) |
| G24 | 扩展格式 | 完成 | 专项规格规定的实现与检查。[记录](../acceptance/g24/2026-10-12.md) |
| G14 | 二维内容 | 完成 | 专项规格规定的实现与检查。[记录](../acceptance/g14/2026-10-12.md) |
| B1 | 音频关闭问题收尾 | Ready | 本阶段交付。[记录](../acceptance/b1/2026-10-12.md) |
| P9 | 内容扩展集成交付收尾 | Ready | 本阶段交付。[记录](../acceptance/p9/2026-10-12.md) |
| UI1 | 启动与项目中心 | 完成 | 该批实现与自动检查，实体复核归属 P8。[记录](../acceptance/ui1/2026-10-12.md) |
| UI2 | 项目隔离与游戏入口 | 完成 | 该批实现与自动检查，实体复核归属 P8。[记录](../acceptance/ui2/2026-10-12.md) |
| UI3 | 设计系统与发布界面 | 完成 | 该批实现与自动检查，实体复核归属 P8。[记录](../acceptance/ui3/2026-10-12.md) |
| UI4 | 安装版制作发布 | 完成 | 该批实现与自动检查，实体复核归属 P8。[记录](../acceptance/ui4/2026-10-12.md) |
| UI5 | 资源与日志面板 | 完成 | 该批实现与自动检查，实体复核归属 P8。[记录](../acceptance/ui5/2026-10-12.md) |
| UI6 | 层级与属性面板 | 完成 | 该批实现与自动检查，实体复核归属 P8。[记录](../acceptance/ui6/2026-10-12.md) |
| UI7 | 场景视口与工具栏 | 完成 | 该批实现与自动检查，实体复核归属 P8。[记录](../acceptance/ui7/2026-10-12.md) |
| UI8 | 工作区集中交付检查 | 完成 | 该批实现与自动检查，实体复核归属 P8。[记录](../acceptance/ui8/2026-10-12.md) |
| R10 | 渲染扩展候选 | 待完成 | 候选规格与实施范围待落实。[记录](hazel-fermion-manifest.json) |
| WF1 | 阶段状态与一致性检查 | 完成 | 本阶段交付。[记录](../acceptance/wf1/2026-10-12.md) |
| WF2 | 验收执行队列与准备报告 | 完成 | 本阶段交付。[记录](../acceptance/wf2/2026-10-13.md) |
| ED0 | 源码审查与职责契约 | 完成 | 本轮开发交付及受影响短检查，正式验收归属 P8。[记录](../acceptance/ed0/2026-10-14.md) |
| ED1 | 基础创建、命名与碰撞策略 | 完成 | 本轮开发交付及受影响短检查，正式验收归属 P8。[记录](../acceptance/ed1/2026-10-14.md) |
| ED2 | 层级与对象操作 | 完成 | 本轮开发交付及受影响短检查，正式验收归属 P8。[记录](../acceptance/ed2/2026-10-14.md) |
| ED3 | 组件生命周期与属性引用 | 完成 | 本轮开发交付及受影响短检查，正式验收归属 P8。[记录](../acceptance/ed3/2026-10-14.md) |
| ED4 | 外部拖放与视口交互 | 完成 | 本轮开发交付及受影响短检查，正式验收归属 P8。[记录](../acceptance/ed4/2026-10-14.md) |
| ED5 | 设置分层与统一工作区 | 完成 | 本轮开发交付及受影响短检查，正式验收归属 P8。[记录](../acceptance/ed5/2026-10-14.md) |
| ED6 | 可选渲染特性与项目隔离 | 完成 | 本轮开发交付及受影响短检查，正式验收归属 P8。[记录](../acceptance/ed6/2026-10-14.md) |
| ED7 | 制作集成与交付准备 | 完成 | 本轮开发交付及受影响短检查，正式验收归属 P8。[记录](../acceptance/ed7/2026-10-14.md) |
| P8 | 最终交付验收 | Ready | 开发与机器批次已整理，实体操作及其余正式条件待满足。[记录](../acceptance/p8/2026-10-14.md) |
| UX-A | 创建与层级 | 完成 | Unity 操作习惯与通用编辑器职责。[记录](../editor-usability-audit.md) |
| UX-B | 工作区与视口 | 完成 | Unity 操作习惯与通用编辑器职责。[记录](../editor-usability-audit.md) |
| UX-C | 组件与脚本 | 完成 | Unity 操作习惯与通用编辑器职责。[记录](../editor-usability-audit.md) |
| UX-D | 设置与扩展归属 | 完成 | Unity 操作习惯与通用编辑器职责。[记录](../editor-usability-audit.md) |
| UX-E | 集中交互复核 | Ready | Unity 操作习惯与通用编辑器职责。[记录](../acceptance/ux/2026-10-14.md) |
| VD0 | 参考研究与视觉设计 | 完成 | 界面现代化与职责分层。[记录](2026-10-14-ui-modernization.md) |
| VD1 | 公共界面基础 | 完成 | 界面现代化与职责分层。[记录](2026-10-14-ui-modernization.md) |
| VD2 | 品牌与项目中心 | 完成 | 界面现代化与职责分层。[记录](2026-10-14-ui-modernization.md) |
| VD3 | 工作台与视口外壳 | 完成 | 界面现代化与职责分层。[记录](2026-10-14-ui-modernization.md) |
| VD4 | 层级属性与资源面板 | 完成 | 界面现代化与职责分层。[记录](2026-10-14-ui-modernization.md) |
| VD5 | 设置制作与发布界面 | 完成 | 界面现代化与职责分层。[记录](2026-10-14-ui-modernization.md) |
| VD6 | 集中验证与交付 | 开发中 | 界面现代化与职责分层。[记录](2026-10-14-ui-modernization.md) |

## 最终收尾

已授权移交的条件由 P8 持有，原通过标准保持有效。
历史失败、检查结果和候选指纹按原记录读取。
P8 完成前，项目的完整交付验收仍待完成。

| 条目 | 来源阶段 | 收尾阶段 | 当前结果 |
| --- | --- | --- | --- |
| P5-causality | P5 | P8 | 待验 |
| P5-performance | P5 | P8 | 失败记录保留，待复核 |
| P5-long-run | P5 | P8 | 通过 |
| P5-delivery | P5 | P8 | 待验 |
| UX-008-causality | U16 | P8 | 待验 |
| P4-input | P4 | P8 | 未执行 |
| P5-D-gameplay | P5 | P8 | 未执行 |
| P5-D-audio | P5 | P8 | 未执行 |
| U15-baseline | U15 | P8 | 未执行 |
| U16-recovery | U16 | P8 | 未执行 |
| P6-authoring | P6 | P8 | 未执行 |
| G20-audio | G20 | P8 | 未执行 |
| U17-transfer | U17 | P8 | 未执行 |
| P7-exchange | P7 | P8 | 未执行 |
| G23-audio | G23 | P8 | 未执行 |
| P8-final | P8 | P8 | 未执行 |
| UI8-input-ime | UI8 | P8 | 未执行 |
| UI8-directory-causality | UI8 | P8 | 待验 |
| ED-workflow | ED7 | P8 | 未执行 |
| UX-E-HUMAN | UX-E | UX-E | 待验 |
| VD6-HUMAN | VD6 | VD6 | 待验 |

详细步骤见[验收收尾流程](../acceptance/phase-closure.md)。
