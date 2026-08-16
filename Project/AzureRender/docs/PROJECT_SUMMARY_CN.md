# AzureRender 项目总结

> 最后核对：2026-08-16（Asia/Shanghai）
> 版本：`0.1.0-rc1` ｜ 仓库：`D:/Assigment/2609/FYP/`（主工程 `Project/AzureRender`）
> 测试：CTest 11/11 通过 ｜ 构建：Debug + Release 双平台可编译

## 1. 项目定位

Portfolio-first 的 Vulkan 风格化渲染器,目标场景为卡通角色 + 工业科幻展示。
FYP 研究主线为《Comparative Evaluation of Vulkan Subpasses and Dynamic
Rendering Local Read》(论文三路径基准,当前 Deferred)。

## 2. 技术栈

| 项 | 选型 |
|---|---|
| 图形 API | Vulkan 1.4(VK_KHR_dynamic_rendering 兼容路径),Validation 全程开启 |
| 语言/标准 | C++17 |
| 窗口/平台 | GLFW |
| 资产加载 | tinygltf(内置 stb_image 解码) |
| UI | Dear ImGui(Docking) |
| 数学 | 自研轻量矩阵/向量(`AzureRenderInternal.hpp`) |
| 依赖管理 | vcpkg(glfw3 / stb / tinygltf) |
| 构建 | CMake Presets(ninja-debug / ninja-release) |

## 3. 已交付能力(按里程碑)

### 渲染核心(M1/M2)
- 卡通 Ramp/Shadow v1:渲染器自持 10 行 Ramp Atlas(Skin/Face/Hair/Fabric/Metal/Eye)
- 直接漫反射、环境、阴影可见性、AO、材质阴影染色、风格遮罩路由独立 QA 视图
- M2 Hero 质量门禁冻结;Validation 发布门禁

### 工程化基线(AR-0 ~ AR-4.5)
- `RenderSettings` 版本化契约,CLI/场景/渲染共用
- `.azscene v1` 原子保存(临时文件 + rename)
- 进程内扩展注册中心(Feature / Importer / Panel)
- `ResourceLocator` 开发/安装树统一
- Windows/Linux 持续集成门禁 + 可移动 RC 发布包

### v2 固定队列(AR-5.x)——RC1 审计完成
- **AR-5.1** 类型化 CLI 解析 + 稳定错误码契约
- **AR-5.2** 场景原子保存 + 恢复测试
- **AR-5.3** 统一运行日志(全部 std::cout/cerr 汇入 RuntimeDiagnostics)
- **AR-5.4** GPU 能力报告 JSON(nlohmann::json + Schema v1)
- **AR-5.5** 发布合规清单(许可证正文 + 文件级 SHA-256 可复现校验)
- **AR-5.6** RC1 版本冻结(`0.1.0-rc1`)+ 双平台审计报告

### v3 固定队列(MAINT-1 + AR-6/7)——全部完成
- **MAINT-1** 仓库健康修复:临时 clone 替换损坏 `.git`,fsck 清零;`.workbuddy/` 移出版本控制
- **AR-6.1** HDR IBL:程序化 HDR equirectangular(RGBA16F)+ blit mip 链
- **AR-6.2** Morph Target:glTF POSITION targets + push constant 权重混合
- **AR-6.3** 逐三角形透明排序(替代 primitive 级中心排序)
- **AR-7.1** 视口对象拾取(Möller–Trumbore 射线求交 + 选中高亮)
- **AR-7.2** 变换 Gizmo(push constant mat4 + Inspector 拖拽编辑)
- **AR-7.3** 场景图编辑(Outliner 层级树 + 增删 + Reload)

### v4 打磨队列(AR-8.x)——全部完成
- **AR-8.1** HDR IBL 精化:外部 equirect 资产导入(`--environment`,stb_image 解码 .hdr/.png)+ 测试资产
- **AR-8.2** 视口内 3D Gizmo 手柄(屏幕投影三轴 + 拖拽平移,命中不触发拾取)
- **AR-8.3** ECS 基础:Entity/ComponentArray/World/System + SceneNode 桥接

## 4. 当前测试覆盖(CTest 11/11)

| 测试 | 覆盖点 |
|---|---|
| CommandLine | CLI 参数解析/错误码契约 |
| SceneModel | `.azscene` 原子保存/加载往返 |
| EditorSession | 节点增删、保存/重载、Reload 命令 |
| EditorCameraController | 轨道/平移/缩放数学 |
| Ecs | 实体创建/组件读写/System 执行/ID 复用 |
| RuntimeDiagnostics | 统一日志源 |
| GpuCapabilityReport | GPU 报告 JSON 符合 Schema |
| ResourceLocator | 资源根/安装树定位 |
| ExtensionRegistry | 进程内注册中心 |
| InstallManifestRoundTrip | 安装清单正反向往返 + 篡改负向 |

## 5. 如何验收(实际运行)

```bash
cd "D:/Assigment/2609/FYP/Project/AzureRender"

# 1) 全量自动化测试
ctest --test-dir build/ninja-debug            # 期望 11/11 passed

# 2) 程序化环境 vs 真实 HDR 对比截图
./build/ninja-debug/AzureRender.exe --asset assets_public/test_model.gltf \
    --width 640 --height 360 --capture-dir C:/tmp/cap_procedural \
    --capture-frames 1 --capture-fps 60
./build/ninja-debug/AzureRender.exe --asset assets_public/test_model.gltf \
    --environment assets_public/test_env.hdr \
    --width 640 --height 360 --capture-dir C:/tmp/cap_hdr \
    --capture-frames 1 --capture-fps 60
# 对比 frame_000000.png: 程序化=蓝天太阳; HDR=紫蓝渐变

# 3) 编辑器交互闭环
./build/ninja-debug/AzureRender.exe --asset assets_public/test_model.gltf \
    --editor C:/tmp/demo.azscene --width 1280 --height 720
#   - 左键点击模型 → 橙色选中高亮(拾取 AR-7.1)
#   - 拖拽视口红/绿/蓝三轴端点 → 对象平移(Gizmo AR-8.2)
#   - Inspector DragFloat 平移/旋转/缩放 → 实时变换(AR-7.2)
#   - Outliner 展开树 + Add Child/Delete + Ctrl+S + Reload(AR-7.3)

# 4) Release 端到端
cmake --build --preset ninja-release
ctest --test-dir build/ninja-release
./build/ninja-release/AzureRender.exe --version   # AzureRender 0.1.0-rc1
```

## 6. 明确暂缓 / 后续方向

- **M3 / SC 工业场景** — Deferred(需用户授权)
- **论文三路径**(Traditional / Subpasses / DRLR 基准)— Deferred(FYP 研究核心)
- **动态插件 / 完整 ECS 系统 / Android** — Deferred
- 可继续打磨:specular IBL 真实 compute 卷积、Gizmo 旋转/缩放视口手柄、
  ECS Archetype 优化

## 7. 关键文档入口

- 近期计划与任务状态:`docs/ACTIVE_DEVELOPMENT_PLAN_CN.md`
- 交接摘要:`docs/PROJECT_HANDOFF_CN.md`
- 目录结构:`docs/PROJECT_STRUCTURE_CN.md`
- RC1 审计:`docs/RC1_AUDIT_CN.md` ｜ 开发日志:`docs/DEVELOPMENT_LOG_CN.md`
