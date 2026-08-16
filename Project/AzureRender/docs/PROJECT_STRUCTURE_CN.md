# AzureRender 项目目录结构

> 最后核对：2026-08-16（Asia/Shanghai）
> 仓库根：`D:/Assigment/2609/FYP/`（git 仓库位于 FYP 根,主工程在 `Project/AzureRender`）
> 统计：`src/` + `tests/` 共约 12.9k 行 C++；11 个 CTest；61 个提交

```
Project/AzureRender/
├── CMakeLists.txt              # 主构建脚本(含 11 个 CTest 注册)
├── CMakePresets.json           # ninja-debug / ninja-release 预设
├── vcpkg.json                  # 依赖: glfw3 / stb / tinygltf
├── README.md                   # 英文项目概述(当前状态同步)
│
├── src/                        # 主源代码(约 12.9k 行 C++)
│   ├── main.cpp                # 入口: CLI 解析 + 场景加载 + App 运行
│   ├── app/                    # 渲染器核心与编辑器桥接
│   │   ├── AzureRenderApp.{hpp,cpp}     # 应用生命周期、资源/管线/帧编排
│   │   ├── AzureRenderFrame.cpp         # 每帧录制: 相机、绘制、拾取、Gizmo 投影
│   │   ├── AzureRenderResources.cpp     # 纹理/缓冲创建、HDR 环境导入
│   │   ├── AzureRenderPipeline.cpp      # 图形管线与 push constant 布局
│   │   ├── AzureRenderDescriptors.cpp   # descriptor set 布局/池
│   │   ├── AzureRenderSupport.cpp       # createImage/mipmap/命令缓冲工具
│   │   ├── AzureRenderCapture.cpp       # 确定性截图/GPU 报告
│   │   ├── AzureRenderOptions.hpp       # 运行选项(含 --environment)
│   │   ├── AzureRenderInternal.hpp      # 向量/矩阵数学辅助
│   │   └── CommandLine.{hpp,cpp}        # 类型化 CLI 解析(AR-5.1 契约)
│   ├── assets/                 # 资产加载
│   │   └── GltfLoader.{hpp,cpp}         # tinygltf 加载、morph target、动画
│   ├── diagnostics/            # 诊断(AR-4.1 / AR-5.3)
│   │   ├── RuntimeDiagnostics.{hpp,cpp} # 统一日志源
│   │   └── GpuCapabilityReport.{hpp,cpp}# GPU JSON 报告(AR-5.4)
│   ├── ecs/                    # ECS 基础(AR-8.3)
│   │   ├── Entity.hpp                  # Entity = uint32 句柄
│   │   ├── IComponentArray.hpp         # 类型擦除组件存储接口
│   │   ├── ComponentArray.hpp          # 每类型组件存储
│   │   └── World.hpp                   # Entity/System/组件池管理
│   ├── editor/                 # 编辑器(AR-3.x / AR-7.x / AR-8.x)
│   │   ├── EditorCameraController.{hpp,cpp}  # 轨道/平移/缩放 + 拾取输入
│   │   ├── EditorContext.{hpp,cpp}     # 场景会话、Gizmo 状态、ECS 桥接
│   │   ├── EditorSession.{hpp,cpp}     # Save/Reload/ResetLayout 命令
│   │   ├── ImGuiEditorLayer.{hpp,cpp}  # Viewport/Outliner/Inspector/菜单
│   │   ├── SceneModel.{hpp,cpp}        # .azscene v1 模型 + 原子保存
│   │   └── IEditorPanel.hpp            # 面板抽象
│   ├── extensions/             # 进程内扩展注册(AR-4.3)
│   │   ├── ExtensionRegistry.hpp
│   │   ├── IAssetImporter.hpp
│   │   └── IRenderFeature.hpp
│   ├── platform/               # GLFW 平台前端(AR-1.1)
│   │   └── GlfwFrontend.{hpp,cpp}
│   ├── render/                 # 渲染设置契约(AR-0)
│   │   ├── RendererCore.{hpp,cpp}
│   │   └── RenderSettings.{hpp,cpp}    # 版本化设置(含 morphWeights)
│   └── resources/              # 资源定位(AR-4.2)
│       └── ResourceLocator.{hpp,cpp}
│
├── shaders/                    # GLSL 450(SPIR-V 构建期编译)
│   ├── mesh.{vert,frag}        # 主材质: HDR IBL / morph / 拾取高亮 / Gizmo 变换
│   ├── background.{vert,frag}  # 背景(环境)
│   ├── shadow.vert + shadow.frag       # 阴影 pass
│   ├── outline.{vert,frag} + inner_outline.{vert,frag}  # 描边
│   └── hud.{vert,frag}         # HUD/文本
│
├── tests/                      # 9 个独立测试程序(11 个 CTest)
│   ├── CommandLineTests.cpp           # CLI 契约(AR-5.1)
│   ├── EcsTests.cpp                   # ECS 实体/组件/系统(AR-8.3)
│   ├── EditorCameraControllerTests.cpp
│   ├── EditorSessionTests.cpp         # 节点增删/保存/重载/Reload(AR-7.3)
│   ├── ExtensionRegistryTests.cpp
│   ├── GpuCapabilityReportTests.cpp   # GPU 报告 Schema(AR-5.4)
│   ├── ResourceLocatorTests.cpp
│   ├── RuntimeDiagnosticsTests.cpp    # 统一日志(AR-5.3)
│   └── SceneModelTests.cpp            # .azscene 原子保存(AR-5.2)
│
├── assets_public/              # 公共资产(随仓库分发)
│   ├── test_model.gltf         # 测试模型(含 BLEND/MASK 材质)
│   ├── test_env.hdr            # HDR 环境测试资产(AR-8.1 生成)
│   ├── toon_ramp_atlas.ppm     # 卡通渐变条
│   ├── toon_ramp_profiles.json
│   └── face_sdf_v1.png
│
├── assets_private/             # 私有角色资产(不随发布包分发)
├── assets_placeholder/         # 占位资产说明
│
├── docs/                       # 项目文档(中文为主)
│   ├── PROJECT_HANDOFF_CN.md           # 交接摘要(当前节点)
│   ├── ACTIVE_DEVELOPMENT_PLAN_CN.md   # 近期执行计划(v6, v4 队列完成)
│   ├── DEVELOPMENT_LOG_CN.md           # 开发日志
│   ├── RENDERER_MODULARIZATION_PLAN_CN.md
│   ├── RC0_BASELINE_CN.md / RC_RELEASE_CN.md / RC1_AUDIT_CN.md  # 发布审计
│   ├── CLI_CONTRACT_CN.md              # CLI 契约(AR-5.1)
│   ├── HDR_TONEMAPPING_DESIGN_CN.md
│   ├── TOON_RAMP_SHADOW_V1_CN.md / MATERIAL_SYSTEM_V1_CN.md
│   ├── CHARACTER_QA_HARNESS_CN.md / LAEVAT_ASSET_EXPORT_CN.md
│   └── PROJECT_STRUCTURE_CN.md / PROJECT_SUMMARY_CN.md  # 本文档与总结
│
├── schemas/                    # JSON Schema
│   ├── azure_render_material.schema.json
│   └── gpu_capability_report.schema.json
│
├── third_party/                # vendored 依赖
│   └── imgui/                  # Dear ImGui(编辑器 UI)
│
├── tools/                      # 工具脚本
│   ├── run_release_gate.cmake  # 发布门禁(AR-5.0)
│   ├── write_install_manifest.cmake / verify_install_manifest.cmake  # AR-5.5
│   ├── test_install_manifest_roundtrip.cmake
│   ├── run_character_qa.ps1 / build_qa_contact_sheet.py
│   ├── build_toon_ramp_atlas.py / validate_material_profiles.py
│   ├── unreal_*.py / *.js      # 资产导出/注入工具
│   └── check_active_plan.sh
│
├── portfolio/                  # 作品集输出
├── captures/                   # 截图/捕获输出(本地)
├── build/                      # CMake 构建产物(忽略)
└── .workbuddy/                 # 本地工作数据(已从版本控制排除)
```

## 关键模块速查

| 能力 | 入口 | 里程碑 |
|---|---|---|
| 渲染管线 | `src/app/AzureRenderApp.cpp` + `shaders/mesh.*` | M2 Hero 冻结 |
| HDR IBL | `AzureRenderResources.cpp` (`loadEnvironmentAsset` / mip 链) | AR-6.1 / AR-8.1 |
| Morph Target | `GltfLoader.cpp` + `mesh.vert` | AR-6.2 |
| 逐三角形 OIT | `AzureRenderFrame.cpp` | AR-6.3 |
| 对象拾取 | `EditorContext` + `AzureRenderFrame::pickPrimitive` | AR-7.1 |
| 变换 Gizmo | `EditorContext::GizmoScreenData` + `ImGuiEditorLayer` | AR-7.2 / AR-8.2 |
| Scene Graph | `SceneModel` + `ImGuiEditorLayer::drawOutlinerPanel` | AR-7.3 |
| ECS | `src/ecs/World.hpp` | AR-8.3 |
| 统一日志 | `RuntimeDiagnostics` | AR-5.3 |
| 发布门禁 | `tools/run_release_gate.cmake` | AR-5.0 / AR-5.5 |
