# AzureRender 业务概念索引

> 按需求关键词直查，找到 module_id 后再读对应 `_overview.md`。
> **每行都是独立可检索的业务映射**，避免跨行拼读。

| 业务概念/需求关键词 | module_id | 子文档 | 关键符号 | 一句话说明 |
|------------------|-----------|--------|---------|-----------|
| 风格化角色渲染 / Toon / 描边 | Project | `Project/Project_character.md` | `CharacterSceneRenderer`, `shaders/mesh.frag` | 角色分层光照与内外描边的唯一实现位置，改这里直接影响角色基线与作品集证据 |
| 脸 / Face SDF / 面部阴影 | Project | `Project/Project_character.md` | `AssetFaceSdfProfile`, `RenderSettings.faceSdf` | 面部 SDF 资产与设置共同决定脸颊阴影，开启后无变化先查 Profile 是否被材质分类接纳 |
| 头发 / Hair KK / 各向异性高光 | Project | `Project/Project_character.md` | `MaterialFeatureHairAnisotropy` | 发丝高光条带由 KK 模型与头发参数决定，改纹理槽位起点会让部分材质采错纹理 |
| 聚簇光照 / 点光源上限 | Project | `Project/Project_character.md` | `kClusterGridX/Y/Z`, `kMaxSceneLights` | 16×9×24 聚簇网格与 128 光源上限，改网格或上限同时影响级联 viewport 断言 |
| 级联阴影 / PCSS / 软阴影 | Project | `Project/Project_character.md` | `kShadowCascadeCount`, `computeCascadeSplits` | 4 级级联配 12 点 blocker 与 16 点 PCF，属已验收视觉特征，过滤参数与级联配置必须分离 |
| 材质分类 / Material Class / Profile | Project | `Project/Project_character.md` | `AssetMaterialClass`, `materialProfileVersion` | 十类材质决定着色分支与 Feature 位，改取值会让已发布私有资产的 Profile 失效或错分类 |
| 黑洞 / 吸积盘 / 引力红移 | Project | `Project/Project_blackhole.md` | `BlackholeSceneRenderer`, `BlackholeQuality` | 全屏光线积分加多普勒与红移近似，正面与近距离对比依赖同一份确定性基线 |
| TAA / 时间累积 / History 重置 | Project | `Project/Project_blackhole.md` | `TaaUniform::blendWeight`, `onSwapchainRecreate` | 相机切换与交换链重建必须重置累积，少一处留拖影、多一处 TAA 完全失效 |
| 渲染图 / Pass 顺序 / 图像屏障 | Project | `Project/Project_render_core.md` | `RenderGraph::compile`, `ImageBarrierDesc` | 资源声明决定屏障生成，帧内资源只能经 `addResource/importImage`，私自律托会漏屏障 |
| RHI / 后端抽象 / NullRhi | Project | `Project/Project_render_core.md` | `IRhi`, `NullRhi`, `VulkanRhi` | 两个后端共享同一屏障校验，新增字段须同步两端断言，否则测试通过而真机黑屏 |
| 画质设置 / RenderSettings / 版本迁移 | Project | `Project/Project_render_core.md` | `RenderSettings::kSchemaVersion`, `migrateRenderSettings` | 当前 v7 且随 `.azscene` 持久化，新增字段必须给默认值，禁止靠忽略未知字段伪造兼容 |
| 着色器绑定 / Descriptor / Push Constant | Project | `Project/Project_render_core.md` | `MaterialPushConstants`, 最终 Composite Descriptor | 改一个 Binding 要同步 GLSL、Layout、Pool、写入与参考表，缺一项即黑图或错采样 |
| 宿主 / 帧循环 / 交换链重建 | Project | `Project/Project_host.md` | `kMaxFramesInFlight`, `AzureRenderFrame.cpp:drawFrame` | 宿主持有全部 Vulkan 对象，帧槽为 2，改帧槽或公共 Attachment 会波及三个内置场景 |
| 截图 / 确定性捕获 / Capture Manifest | Project | `Project/Project_host.md` | `CaptureRequest`, `appendCaptureManifestFields` | 尺寸 64..7680 × 64..4320、fps 1..240，捕获与重开捕获同一入口，校验不可省略 |
| 命令行参数 / CLI 契约 / 退出码 | Project | `Project/Project_host.md` | `CommandLineErrorCode`(1..4), `--qa-*` 互斥 | 任意 `--qa-*` 不得与 `--technical-sequence` 组合，值域与互斥改动会让门禁脚本失败 |
| 场景文档 / .azscene / 序列化 | Project | `Project/Project_scene_editor.md` | `SceneDocument::kSchemaVersion`, `SceneDocument::save` | 多资源多节点并内嵌 `renderSettings`；空路径抛异常，未知未来版本拒绝加载 |
| 撤销 / Undo / 快照历史 | Project | `Project/Project_scene_editor.md` | `EditorCommand`, 快照上限 100 | 编辑器以快照历史实现 Undo/Redo，改命令集或快捷键须重跑 `EditorSessionTests` |
| 层级变换 / 父节点 / 世界矩阵 | Project | `Project/Project_scene_editor.md` | `resolveNodeWorldTransforms` | 返回与节点等长且索引一致的世界矩阵，改这里同时影响角色渲染、编辑器与点光源聚簇 |
| 新增渲染器 / Renderer SDK | Project | `Project/Project_renderer_sdk.md` | `ISceneRenderer`, `ExtensionRegistry::registerFactory` | 须实现生命周期与 `capabilities()` 并在 `BuiltinRendererCatalog` 注册，Registry 拒绝空 ID 与重复 ID |
| 构建 / CMake / vcpkg / 双工具链 | Project | `Project/Project_build_release.md` | `CMakePresets.json`, `vcpkg.json` | MinGW 与 MSVC 两条 ABI 路径都要验证；imgui 由源码编译，改用包管理器版本会链接失败 |
| 资源定位 / 开发树 / 安装树 | Project | `Project/Project_build_release.md` | `ResourceLocator`, `AZURERENDER_RESOURCE_ROOT` | 搜索顺序改动后开发态能跑、安装态找不到资源，唯一有效检出是 `--check-resources` 与隔离运行 |
| 发布门禁 / 打包 / 安装清单 | Project | `Project/Project_build_release.md` | `run_release_gate.cmake`, `verify_install_manifest.cmake` | 九阶段序列定义验收口径，禁止以单独 `cmake --build` 绕过脚本认定通过 |
| 视觉回归 / 基线 / 容差 | Project | `Project/Project_build_release.md` | `visual_regression_cases.json` | 严格容差全零、宽松为 0.006/0.02/0.0078431373；CI 内禁止写入或更新基线 |
| CI / 持续集成 / 合并门禁 | .github | `.github/github_ci_pipeline.md` | `.github/workflows/ci.yml` | Linux 与 Windows 双平台构建、CTest、发布门禁、视觉回归与安装包产出，失败阻断合并 |
| 文档站点 / GitHub Pages / 文风检查 | .github | `.github/github_docs_publish.md` | `documentation.yml`, `check_doc_style.py` | 路径白名单触发，句长上限 45 字、段落最多 4 句；站点地址是包内 README 的链接契约 |
| 仓库入口 / 文档导航 | __files | `__root/__files/__files_entry.md` | `README.md`, 8 条主题入口 | 根 README 只做导航不复述技术细节，新增主题页必须登记进编号清单，否则成为孤儿页 |
| 资产边界 / 提交过滤 / 凭据安全 | __files | `__root/__files/__files_policy.md` | `.gitignore`, `assets_public/`, `assets_private/` | 构建产物、捕获、凭据与本地参考代码不进入版本控制，新增忽略规则只写仓库根 `.gitignore` |
| Agent 工具协约 / 检索方式 | __files | `__root/__files/__files_policy.md` | `AGENTS.md`, Codemap MCP | 检索一律走 Codemap MCP，取得符号详情后立即编辑；子代理必须用 `general` 而非 `explore` |
| 公共后处理 / Bloom / HUD | Project | `Project/Project_host.md` | `PostProcessPushConstants`, `kMaxHudVertices` | 帧内公共 Pass 必须写在 `AzureRenderFrame.cpp:drawFrame`，不得放进某个场景 Renderer |
| 私有资产 / 安装包内容边界 | Project | `Project/Project_build_release.md` | `assets_private/`, `captures/` | 私有模型、纹理与派生媒体禁止进入版本库、CI、安装包与作品集 |
