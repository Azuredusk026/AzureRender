# 论文三路径基准实验设计(FYP)

> 版本：2026-08-16 v1 ｜ 研究主题：Vulkan Subpasses 与 Dynamic Rendering Local Read 对比

## 1. 研究问题

在风格化渲染管线(Shadow → Main Scene → Post-Process)中,比较三种
执行模型的开销与可移植性:

| 路径 | 执行模型 | 关键特性 |
|---|---|---|
| **Traditional** | 独立 VkRenderPass × 3 | 附件 store/load,pass 间显式 layout 转换 |
| **Subpasses** | 单 RenderPass 多 Subpass | framebuffer-local 读取,免显式转换 |
| **DynamicRendering** | vkCmdBeginRendering(Vulkan 1.3) | 无 VkRenderPass 对象,动态设置 |
| **DRLR**(后续) | vkCmdBeginRendering + local read | VK_KHR_dynamic_rendering_local_read |

## 2. 已实现(AR-9.2)

- `RenderSettings::RenderPath` 枚举 + CLI `--render-path <traditional|subpasses|dynamic>`;
- GPU timing JSON 报告增加 `renderPath` 字段;
- **每帧明细 CSV**(`--gpu-timing-output <path>` → `<path>.csv`):
  列 = `frame, renderPath, shadowMs, sceneMs, postProcessMs, frameMs`;
- 当前 Subpasses/DynamicRendering 已接入设置,渲染执行仍走传统路径
  (三套渲染器实际实现列入后续阶段)。

## 3. 测量协议

```bash
# 三条路径各跑 N 帧,输出 JSON + CSV
./build/ninja-debug/AzureRender.exe --asset assets_public/test_model.gltf \
  --smoke-frames 300 --gpu-timing --gpu-timing-output C:/tmp/traditional.json
./build/ninja-debug/AzureRender.exe --asset assets_public/test_model.gltf \
  --smoke-frames 300 --gpu-timing --gpu-timing-output C:/tmp/subpasses.json \
  --render-path subpasses
./build/ninja-debug/AzureRender.exe --asset assets_public/test_model.gltf \
  --smoke-frames 300 --gpu-timing --gpu-timing-output C:/tmp/dynamic.json \
  --render-path dynamic
```

- 预热:丢弃前 10 帧(驱动缓存/管线编译);
- 统计:中位数而非均值(避免 GC/OS 抖动),报告 p50/p95/max;
- 固定相机/动画:同一 --qa-camera 预设保证场景逐帧一致;
- 双 GPU/平台重复 ≥3 次,报告均值±标准差。

## 4. 分析指标

- `frameMs` 整体对比(主要结论);
- `sceneMs`(主场景)作为附件访问模型差异的代理;
- 扩展可用性:Vulkan 1.3 核心 vs 扩展门控 —— 可移植性讨论。

## 5. 状态

| 项 | 状态 |
|---|---|
| 三路径抽象 + CLI | ✅ Complete |
| 基准 CSV/JSON 输出 | ✅ Complete |
| Traditional 路径真实执行 | ✅ Complete |
| Subpasses 渲染器实现 | 🔜 后续 |
| DynamicRendering 渲染器实现 | 🔜 后续 |
| DRLR 扩展实现 | 🔜 后续(需 VK_KHR_dynamic_rendering_local_read) |

## 6. 参考提交

- `完成 AR-9.2 论文三路径基准`(框架 + 测量管道)
