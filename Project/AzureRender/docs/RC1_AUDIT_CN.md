# AzureRender RC1 发布审计

> 审计版本：RC1（0.1.0-rc1）
> 审计日期：2026-08-16
> 审计节点：AR-5.6（v2 固定执行队列末节点）

## 1. 版本冻结

- 项目版本：`0.1.0-rc1`（CMake `PROJECT_VERSION` 保持 0.1.0，RC 后缀由
  `AZURERENDER_VERSION` 与 CPack 包名统一携带）；
- 冻结范围：CLI 契约（v1）、`.azscene v1`、`RenderSettings v2`、Capture
  Manifest v1、GPU 能力报告 Schema v1、安装树内容清单 Schema v1；
- 行为基线：RC0（`docs/RC0_BASELINE_CN.md`）冻结的全部输入/输出契约在
  RC1 保持不变，仅新增诊断、合规与打包能力。

## 2. 固定执行队列完成情况

| 节点 | 状态 | Commit | 验证证据 |
|---|---|---|---|
| AR-3.6 | Complete | `8f477b2` | Viewport 资源独立重建，Debug/Release + Validation |
| AR-3.7 | Complete | `7413533` | Editor Session 闭环，保存/脏状态测试 |
| AR-4.0 | Complete | `51f220d` | RC0 基线冻结，统一门禁定义 |
| AR-4.1 | Complete | `9d0db28` | 结构化日志、错误码、GPU 能力报告 |
| AR-4.2 | Complete | `e458ceb` | ResourceLocator、安装树、可移动资源 |
| AR-4.3 | Complete | `64b1c19` | 扩展注册中心 |
| AR-4.4 | Complete | `d3ce109` | 跨平台 CI 门禁 |
| AR-4.5 | Complete | `22080b6` | RC 发布包、干净环境验收 |
| AR-5.0 | Complete | `2bd3bb8` | 发布门禁编排 |
| AR-5.1 | Complete | `60e69f3` | 类型化 CLI 契约 |
| AR-5.2 | Complete | `c14cc9e` | `.azscene` 原子保存 |
| AR-5.3 | Complete | `275d5e8` | 统一运行日志 |
| AR-5.4 | Complete | `9521a83` | GPU 报告契约 |
| AR-5.5 | Complete | `7090f64` | 发布合规清单 |
| AR-5.6 | Complete | 本节点 | 本审计 |

## 3. 双平台证据汇总

### 3.1 Windows（本次 RC1 审计，RTX 4060 Laptop GPU）

- Ninja Debug / Release 构建成功；
- 全量 CTest **10/10 通过**：
  - AzureRender.CommandLine
  - AzureRender.EditorCameraController
  - AzureRender.EditorSession
  - AzureRender.SceneModel（AR-5.2 原子保存）
  - AzureRender.RuntimeDiagnostics（含 warning/print 事件流）
  - AzureRender.GpuCapabilityReport（JSON 安全 + Schema）
  - AzureRender.ResourceLocator
  - AzureRender.ExtensionRegistry
  - AzureRender.ReleaseGateMissingBuild（负向门禁）
  - AzureRender.InstallManifestRoundTrip（AR-5.5 合规往返）
- 公共资产 `test_model.gltf` 120 帧 Debug Validation：退出码 0，无 VUID；
- GPU 能力报告 JSON 生成成功并符合 Schema v1；
- 运行日志统一汇入 `captures/azurerender.log.jsonl`；
- 安装树内容清单生成 + 校验通过（许可证正文齐全、哈希可复现）。

### 3.2 Linux（历史 CI 证据）

- AR-4.4 建立 Windows/Linux 双平台流水线：Debug/Release 构建、CTest、
  公共资产 smoke、Xvfb/lavapipe 软件 Vulkan Validation；
- AR-3.x~AR-5.x 各节点在 Linux Debug/Release 构建与公共资产回归中通过；
- Linux 包依赖系统 Vulkan Loader/驱动与 GLFW 运行库（见
  `docs/RC_RELEASE_CN.md`）。

> 注：本次 RC1 审计在 Windows 工作站完成；Linux 证据引用 AR-4.4 起记录的
> CI 结果。RC 包在两个平台均以 `--check-resources` 与 `--smoke-frames` 门禁
> 验证（发布门禁编排 AR-5.0 统一入口）。

## 4. 合规清单（AR-5.5 结果）

安装树 `share/AzureRender/licenses/` 携带以下许可证正文：

- `imgui-LICENSE.txt`（vendored Dear ImGui, MIT）
- `glfw3-LICENSE.txt`（zlib/libpng）
- `tinygltf-LICENSE.txt`（MIT）
- `stb-LICENSE.txt`（MIT/public domain）
- `nlohmann-json-LICENSE.txt`（MIT）

`THIRD_PARTY_NOTICES.md` 与 `install_manifest.json` 同步生成，文件级 SHA-256
可复现校验（`tools/verify_install_manifest.cmake`）。

## 5. 发布审计结论

- 无未提交工作区改动（除 `.workbuddy/` 本地工作数据外）；
- 无私有资产进入发布包（`assets_private/`、`captures/` 隐私检查通过）；
- 全部 v2 固定队列节点独立提交并记录在开发日志；
- 已知限制维持 RC0 声明：无 Android、论文三路径、对象拾取/Gizmo、
  资源导入、动态插件、完整 ECS。

**结论：RC1 满足发布候选审计要求，可以进入用户验收。**

## 6. 后续入口

v2 固定队列全部完成。下一执行顺序由 Active Plan 更新版或用户新授权决定；
M3/SC 工业场景、论文三路径等仍按既有决策 Deferred。
