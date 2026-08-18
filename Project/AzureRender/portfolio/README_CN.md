# AzureRender 作品集交付

本目录保存可公开展示的封面、技术联系表和机器可读清单。大型视频与本地 capture 不纳入版本控制；发布前应由 `portfolio_manifest.json` 校验实际交付文件。

## 展示顺序

1. `images/azurerender_cover_1920x1080.png`：项目名称与角色 Beauty。
2. 角色 Beauty 视频：展示材质、GPU skinning、动画和固定环绕镜头。
3. 技术拆解视频：依次说明 World Normal、Internal Outline、Shadow Map 与 GPU HUD。
4. `images/technical_contact_sheet_1920x1080.png`：静态技术总览。
5. 黑洞场景：展示可插拔 `ISceneRenderer` 架构和 GPU 测地线追踪。
6. GPU timing JSON：说明测量范围，不把 pass 合计误称为完整帧时间。

## 项目说明

AzureRender 是自主开发的 C++17/Vulkan 实时渲染器。角色路径支持 glTF、多材质、GPU skinning、动画、HDR IBL、风格化光照、Shadow Map、几何与屏幕空间描边，以及确定性捕获。公共 renderer core 还可以挂载独立场景渲染器；黑洞场景以 fullscreen shader 实现测地线追踪和体积吸积盘。

## 可讲述的技术点

- Vulkan 显式资源生命周期、同步和多 pass 架构。
- `ISceneRenderer` 把公共渲染设施与场景专属 pipeline 分离。
- glTF 材质、skin、animation、morph 和透明排序。
- HDR Scene Color、IBL、ACES fitted tone mapping。
- Shadow、Main Scene、Post-process 和 HUD 的 GPU timestamp。
- 固定时间步 PNG/capture manifest/视频编码交付链。
- 编辑器 Viewport、拾取、Gizmo、Scene Graph 和 ECS 桥接。

## 性能陈述规则

历史角色基准设备为 NVIDIA RTX 4060 Laptop GPU、1920x1080 Release。历史记录中的 Shadow/Main/Outline 合计只覆盖 GPU command buffer 内被 timestamp 包围的 pass，不包含 CPU、present、readback 或编码。

黑洞 BH-2.1 在 1800 steps、4 samples 配置下记录的 main scene 约为 14.1 ms。BH-2.2 完成后必须重新测量，不能沿用旧数字。

## 资产声明

渲染器源码、shader、工具、公共测试资产和程序化动画属于本项目。`assets_private/` 中的角色模型与派生纹理只用于受控技术验证，不得随公开仓库或交付包重新分发。公开演示应明确区分自研渲染技术与第三方美术资产。

## 重新生成

```powershell
powershell.exe -NoProfile -ExecutionPolicy Bypass `
  -File .\tools\build_portfolio_package.ps1 `
  -FfmpegExecutable "C:\path\to\ffmpeg.exe"
```

脚本验证源文件并刷新 `portfolio_manifest.json`。生成前先检查 manifest 中引用的 capture 文件确实存在，并确认输出不包含私有资产。

当前工程状态和发布规则分别见 `../docs/PROJECT_OVERVIEW_CN.md` 与 `../docs/RELEASE_AND_ACCEPTANCE_CN.md`。
