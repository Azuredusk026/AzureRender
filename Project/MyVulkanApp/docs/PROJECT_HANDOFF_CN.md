# 项目开发交接摘要

> 最后核对：2026-07-25（Asia/Singapore）  
> 主工程：`Project/MyVulkanApp`  
> 当前实际节点：**S21 已完成，S22 尚未开始**

## 1. 项目信息

项目暂用名为 **Vulkan Stylized Character Renderer**。FYP 的研究题目是：

> Comparative Evaluation of Vulkan Subpasses and Dynamic Rendering Local Read for Real-Time NPR Rendering

最终研究目标是在相同 NPR 工作负载下，对比传统 Multi-pass、Vulkan Subpasses 和 Dynamic Rendering Local Read（DRLR）。当前代码仍处于“作品集优先”的桌面渲染器阶段，主要用于先建立稳定、可展示的 NPR 视觉基线。

技术栈：

- Windows 10/11、C++17、Vulkan 1.3、GLFW、CMake/Ninja；
- glTF 2.0 资产加载，依赖 tinygltf 与 stb；
- GLSL 由 Vulkan SDK 的 `glslc` 编译为 SPIR-V；
- 当前验证设备为 NVIDIA GeForce RTX 4060 Laptop GPU。

`AfterglowRender/` 是 MIT 许可的学习与架构参考，不是主工程，也不应被整体合并进 FYP 主代码。

## 2. 当前真实实现状态

### 已完成

- Vulkan instance/device/surface/swapchain、Debug Validation Layer、双帧并行、resize/recreate；
- device-local 顶点/索引缓冲、per-frame UBO、descriptor、深度测试；
- glTF/GLB 场景节点与 TRS/Matrix、多个 mesh primitive 和多个材质；
- Base Color、Normal、Metallic-Roughness、Specular/Emissive、自定义 Style Mask、Matcap、Hair Data；
- OPAQUE/MASK/BLEND、double-sided、透明 primitive 按视空间深度排序；
- 自动切线生成、模型 bounds 居中/取景、环境漫反射与近似反射；
- 倒壳描边、toon band、材质阴影色、脸部 Matcap、Kajiya–Kay 风格头发高光；
- 程序化圆形地台、接触压暗、全屏渐变背景、主/辅/轮廓三点式灯光；
- 固定全身/四向/脸部近景机位、风格开关与参数热键；
- Vulkan 交换链原生截图写入 PNG；
- 公共自有测试资产与私有莱万汀静态角色均可运行。

S21 的作品集展示节点已经可用。当前私有角色运行时统计为 **81,487 vertices、284,673 indices、14 primitives、15 materials**；公共测试资产为 **337 vertices、900 indices、3 primitives、4 materials**（均包含运行时追加的地台）。

### 当前渲染架构

当前不是最终论文需要的 deferred benchmark。实际代码使用：

- 一个传统 `VkRenderPass`；
- 一个 subpass；
- 同一 pass 内依次绘制背景、倒壳描边和 forward 材质；
- 六个 shader：background、outline、mesh 各一组 vertex/fragment。

因此目前**没有** G-buffer、可切换的 Multi-pass/Subpass/DRLR 三路径、DRLR feature probe、GPU timestamp benchmark、CSV/JSON 实验输出或 Android 端。不能把当前状态描述成“Subpass/DRLR 对比已经实现”。

### 已知限制

- 当前接触阴影只是地台 UV 压暗，不是 Shadow Map；
- 环境贴图是程序生成的 LDR equirectangular texture，不是 HDR IBL；
- 没有 HDR framebuffer、tone mapping、bloom、mipmap/prefiltered specular；
- 没有 skinning、骨骼动画、morph target；
- BLEND 只按 primitive 排序，没有 per-triangle sorting/OIT；
- Unreal Hair `_HN` 的 RG/BA 语义是基于资产证据的兼容还原，不是母材质逐节点复刻；
- 私有莱万汀资产不能提交或公开分发，授权未确认前只可本地验证；
- README 开头仍写 `S1-S19 baseline`，已落后于实际 S21；
- 工作区根目录的 `.git/` 为空，当前路径不被 Git 识别，因而没有可信的 commit/branch/diff 历史。

## 3. 目录与事实来源

- `src/app/VulkanApp.*`：Vulkan 生命周期、渲染、交互、截图、程序化地台；
- `src/assets/GltfLoader.*`：glTF 数据与自定义材质 extras；
- `shaders/`：当前六个 GLSL shader；
- `tools/`：Unreal 导出、纹理转换与 glTF 注入工具；
- `assets_public/test_model.gltf`：可公开、可回归的自有测试资产；
- `assets_private/`：本地第三方角色及派生资产，禁止提交；
- `docs/DEVELOPMENT_LOG_CN.md`：S7–S21 的实现与 QA 记录；
- `docs/LAEVAT_ASSET_EXPORT_CN.md`：私有角色导出、材质映射和限制；
- 根目录 `FYP_Development_Plan_v1.3.docx`：完整研究路线；其中早期“starter app”描述已过时，实际代码状态以本文件、源码和最新开发日志为准。

## 4. 构建、运行与本次验证

工作站现用 Vulkan SDK：

```text
C:\VulkanSDK\1.4.350.0
```

现有构建目录：

```powershell
cd D:\Assigment\2609\FYP\Project\MyVulkanApp
cmake --build build/ninja-debug
cmake --build build/ninja-release
```

公共资产回归：

```powershell
.\build\ninja-debug\MyVulkanApp.exe --smoke-frames 120
```

私有角色回归：

```powershell
.\build\ninja-release\MyVulkanApp.exe `
  --asset .\assets_private\laevat_static\laevat_static_material.glb `
  --smoke-frames 120
```

2026-07-25 的重新验证结果：

- Debug/Release：构建成功，`ninja: no work to do`；
- 公共资产 Debug：Validation Layer 开启，120 帧成功，进程退出码 0；
- 私有角色 Release：120 帧成功，进程退出码 0；
- 使用 GPU：NVIDIA GeForce RTX 4060 Laptop GPU。

交互键位见 `README.md`。最常用的是 `1` 全身、`5` 脸部近景、`F9` 风格开关、`F12` 截图。

## 5. 接下来建议按此顺序继续

1. **仓库与文档收口**
   - 确认 Git 仓库应放在 workspace root 还是 `Project/`，修复/初始化后再开发；
   - 更新 README 的当前节点为 S21，并保留公共/私有资产边界；
   - 不修改 `AfterglowRender/`，除非任务明确要求查阅参考实现。

2. **S22：有边界地完成展示预设**
   - 增加可重复的灯光/背景预设和一个终末地风格构图；
   - 使用固定热键切换，避免破坏 `1`、`5`、`F9` 和现有截图流程；
   - 同时用公共资产 Debug Validation 与私有角色 Release 各跑 120 帧；
   - 记录全身、近景、F9 对照图，然后停止继续无上限视觉调校。

3. **冻结作品集里程碑 D0**
   - 补 renderer 架构图、pass 图、公开截图/视频和第三方资产声明；
   - 把 S21/S22 作为可展示的 forward NPR 基线，不宣称已完成论文 benchmark。

4. **进入 FYP 研究主线**
   - 先做设备/扩展 feature probe，确认桌面与目标 Android 设备的 DRLR 支持；
   - 定义三路径共享的场景、shader 公式、attachment 格式、分辨率和测量协议；
   - 实现真正的 G-buffer Traditional Multi-pass 基线；
   - 再实现 Subpass 和 DRLR 路径；
   - 加入 GPU timestamps、metadata、CSV/JSON 输出与公平性回归测试。

## 6. 给接手 Agent 的工作规则

- 开始任务前先读本文件、README、开发日志最后一个节点和对应源码；
- 只把源码和实际运行结果当作完成依据，不把旧计划中的未来项当作已实现；
- 每个节点保持小改动：构建 Debug/Release，跑公共资产 Validation，再跑私有资产回归；
- 新功能必须有公共资产 fallback，不能让公开版本依赖 `assets_private/`；
- 不提交私有 GLB、纹理、截图或 Unreal 派生资源；
- 完成节点后同步更新 `README.md`、`DEVELOPMENT_LOG_CN.md` 和本交接摘要。
