# AzureRender RC0 基线

> 版本：RC0（2026-08-14）
> 适用节点：AR-4.0

本文冻结桌面发布候选的行为边界。后续 AR-4.x 只能增加诊断、定位、自动化和打包能力，
不得改变下列输入/输出契约而不提升 RC 版本。

## 支持范围

- Windows 10/11 与 Linux 桌面；C++17、Vulkan 1.3、GLFW、CMake 3.20+；
- Debug 构建启用 Khronos Validation Layer，Release 构建不依赖 Validation Layer；
- 公共回归资产为 `assets_public/test_model.gltf`；私有角色只作为补充证据；
- ImGui 编辑器是可选构建能力，启用 GLFW/Vulkan Backend 和 Docking 后提供 Editor Preview。

## 冻结契约

- CLI：现有资产、场景、编辑器、smoke、Capture、QA、诊断和 HUD 参数保持含义；非法组合返回失败，不静默修正；
- `.azscene v1`：保留文件头、资源 ID、节点大纲、可见性和基础 RenderSettings 字段；未知 schema 必须失败；
- `RenderSettings`：版本化 ABI 与 Capture Manifest 状态哈希保持稳定；新增字段必须向后兼容或提升 schema；
- Capture Manifest：记录资产、GPU、分辨率、帧率、帧数、动画、展示模式、诊断和设置 schema；输出不得覆盖已有文件；
- 编辑器最小面：离屏 Viewport、Outliner、Inspector、Asset Browser、Console、相机交互、保存/关闭保存、脏状态和布局重置。

## 统一门禁

```text
1. Linux Debug + Release + ImGui 构建
2. CTest 全部通过
3. 公共资产普通 Renderer Debug Validation 120 帧
4. 公共 .azscene Editor Debug Validation 120 帧
5. UI 任务检查 Viewport 尺寸、无拉伸/黑边和无 Validation VUID
6. git diff --check，提交前工作树只含当前节点
```

当前容器没有可用 X11/Wayland 窗口服务器时，只能完成构建与 CTest；运行时门禁必须在
带桌面会话的 CI runner 或开发机补齐，不能把缺失窗口环境记录为功能通过。

## 非目标与兼容策略

- M3/SC、对象拾取/Gizmo、资源导入、论文三路径、Android、动态 DLL 插件和脚本系统不属于 RC0；
- Legacy `afterglow*` glTF extras、旧动画名和 Capture v1 读取兼容继续保留；
- 不提交私有资产、生成的构建目录或私有截图；
- 违反契约的改动必须先更新本文、升级 RC 版本并单独提交。
