# AzureRender 发布与验收

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

- 黑洞视觉输出依赖 GPU 截图验收，尚无离屏图像单元测试。
- 正式 Android 支持尚未建立。
- 动态插件 ABI 不属于 RC1。
- 私有角色只能用于受控本地验证。

## 7. 历史审计

RC0、RC1 和 2026-08-16 综合验收原始快照位于 `archive/milestones/`。它们是历史证据，不应继续承载当前版本状态。
