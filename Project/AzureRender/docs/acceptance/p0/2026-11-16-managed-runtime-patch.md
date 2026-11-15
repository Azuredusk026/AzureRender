# 托管运行时验收

托管构建使用 .NET SDK 9.0.320。引擎绑定项目指定运行时版本 9.0.22，构建脚本读取同一版本用于 CoreCLR 产物和 NativeAOT 编译。还原过程执行依赖审计。

Release 和 Debug 各完成八项已注册的验证。覆盖 CoreCLR 和 NativeAOT 的模块加载、脚本运行与宿主工作流，以及 Lua 后端一致性和编辑器托管脚本创作。两种配置的绑定产物均通过清单散列校验。

[运行时安全公告](https://github.com/dotnet/runtime/security/advisories/GHSA-p3jp-9v83-76w9) · [验收产物清单](evidence/2026-11-16-managed-runtime-patch/results.json)
