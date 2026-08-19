# AzureRender 发布与验收

## 2026-08-19 发布准备（未打包）

- 当前候选提交包含吸积盘周期噪声接缝修复、角色眉毛专用 Overlay 通路以及双场景技术展示更新。
- Debug 与 Release 均已从当前源码构建；Debug 12/12 CTest 通过。
- `build/install-debug/bin/AzureRender.exe` 与 `build/install-release/bin/AzureRender.exe` 已重新安装，二者均在隔离系统 PATH 下通过版本、资源和运行库检查，不会出现 MinGW 或 GLFW DLL 缺失。
- Debug 安装树已实际运行 Character 3 帧，Release 安装树已实际运行 Blackhole/Cinematic 3 帧，退出码均为 0；Debug Validation 无错误。
- 活动 `build/` 只保留 `ninja-debug`、`ninja-release`、`install-debug` 和 `install-release`。旧 capture、旧 `dist` 包、媒体编码缓存及视觉验证临时目录已移入本地忽略归档 `showcase/archive_legacy/20260819-发布前临时归档/`，可恢复但不属于发布内容。
- 本轮遵循用户要求没有运行 CPack，也没有生成新的 ZIP、TGZ 或安装包。最终打包必须等待明确指令。

## 2026-08-18 视觉修复候选版验收

- 提交：`8f94080`（角色材质与展示光照）、`cb4e004`（吸积盘多普勒、云噪声与越肩机位）。
- Debug/Release：均完成构建并通过 12/12 CTest；Debug 开启 Validation，Release 关闭 Validation。
- 安装树：`build/install-debug/bin/AzureRender.exe` 与 `build/install-release/bin/AzureRender.exe` 均在隔离 PATH 下通过 `--version`、`--check-resources`，`bin/` 包含 `glfw3.dll`、`libgcc_s_seh-1.dll`、`libstdc++-6.dll`、`libwinpthread-1.dll`。
- 实际运行：Debug 安装树 Character 公共资产 120 帧，Release 安装树 Blackhole/Cinematic/over-shoulder 120 帧，退出码均为 0。
- Release Gate：configure、build、test、install、manifest、version、resources、isolated-runtime、package 全部通过。
- TGZ：`dist/AzureRender-0.1.0-rc1-Windows-AMD64.tar.gz`，9,809,353 bytes，SHA-256 `F2AB4ECB85CFF764C9A7DD2B6EA549A5A9E2E45EACC228B5082D713E1AE2876F`。
- ZIP：`dist/AzureRender-0.1.0-rc1-Windows-AMD64.zip`，9,832,988 bytes，SHA-256 `FC67005D3E882E9B73E79C189716212C3596EE89F2E9CE2D58BDDA429BCE7292`。
- 黑洞媒体：三个机位各 300 帧/5.0 秒，总览 900 帧/15.0 秒；四个 MP4 均完成全文件解码。
- 角色媒体：五张 1920×1080 截图和 300 帧/5.0 秒环绕视频，私有资产媒体仅保留本机 `showcase/`。

## 1. 发布目标

当前版本为 `0.1.0-rc1`。发布包必须在没有源码目录的环境中找到可执行文件、shader、公共资产、许可和必要文档。

## 2. 发布内容

- `AzureRender` 可执行文件和平台运行库
- 已编译 SPIR-V shader
- `assets_public/`
- 快速使用、开发/验收说明
- `THIRD_PARTY_NOTICES.md` 和依赖许可证
- 文件级 SHA-256 install manifest
- 随源码发布时可包含 `portfolio/` 公共视觉证据；二进制安装包不需要包含它

禁止包含：

- `assets_private/`
- build cache、IDE 文件、captures
- 私有角色派生截图、旧阶段视频和 probe 输出
- 本机绝对路径、凭据或临时日志

当前版本已停止跟踪 `assets_private/` 内容，但旧 Git 提交可能仍保存历史对象。若将仓库改为公开，必须发布经过审计的干净历史（新仓库或受控历史重写），不能仅依赖当前 `.gitignore`。

## 3. 构建与打包

```powershell
.\tools\configure_windows.ps1 -Config Debug
.\tools\configure_windows.ps1 -Config Release
ctest --test-dir build\ninja-release --output-on-failure
cmake --install build\ninja-release --prefix build\install-release
cpack --config build\ninja-release\CPackConfig.cmake
```

Windows MinGW 使用 `x64-mingw-dynamic` vcpkg triplet。安装目录 `bin/` 必须同时包含 `AzureRender.exe`、`glfw3.dll` 和三项 MinGW runtime DLL；只分发 EXE 不受支持。

隔离运行验证：

```powershell
.\tools\verify_windows_runtime.ps1 `
  -Executable .\build\install-release\bin\AzureRender.exe
```

通过门禁后的本地交付物统一复制到 `dist/`；该目录不进入 Git。文件名由 CPack 版本、系统和架构生成，不手工改成 `final` 或日期任务名。

也可以使用统一门禁：

```powershell
cmake -DBUILD_DIR="$PWD/build/ninja-release" `
  -DCONFIG=Release `
  -P tools/run_release_gate.cmake
```

## 4. 验收矩阵

| 门禁 | Windows | Linux CI |
|---|---:|---:|
| Debug/Release 构建 | 必须 | 必须 |
| 12 个 CTest | 必须 | 必须 |
| 公共 renderer smoke | 必须 | 必须 |
| 公共 editor smoke | 必须 | 必须 |
| Debug Validation | 必须 | 必须 |
| 安装树资源检查 | 必须 | 必须 |
| 隔离 PATH 运行时检查 | 必须 | 不适用 |
| 私有角色 smoke | 可选补充 | 不执行 |
| 视觉截图 | 视觉变更必须 | 可选 |

## 5. 发布证据

每次候选发布记录：

- commit 和版本号
- 操作系统、GPU、驱动、Vulkan API/SDK
- Debug/Release 构建结果
- CTest 摘要
- Validation smoke 命令和退出码
- 安装 manifest 校验
- 视觉基准哈希及有意变化说明
- `portfolio_manifest.json` 校验结果
- 已知限制和私有资产排除确认

证据优先使用 CI artifact、capture manifest 和机器可读 JSON，不在活动文档粘贴整段终端日志。

## 6. 当前已知限制

- 黑洞已有确定性双 capture 像素比较；1920x1080 Cinematic High 的长 PNG readback 曾在 53/60 帧后停止推进，短捕获与正常渲染不受影响。
- 正式 Android 支持尚未建立。
- 动态插件 ABI 不属于 RC1。
- 私有角色只能用于受控本地验证。

## 7. 历史审计

RC0、RC1 和 2026-08-16 综合验收原始快照位于 `archive/milestones/`。它们是历史证据，不应继续承载当前版本状态。
