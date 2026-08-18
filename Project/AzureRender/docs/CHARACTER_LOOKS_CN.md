# Character Look 与美术验收

## 2026-08-18 视觉修正基线

`Endfield Industrial` 背景已移除双层网格、粗横向分界和右侧灯柱，只保留中性渐变、主体 halo 与弱地面分界。该 preset 的角色补光和环境漫反射同步提高，暗色服装仍保持层次，但不得再压成无细节黑块。

## 头发视觉基线（2026-08-18）

本轮对照参考图和 `Endfield角色渲染复刻技术文档.md` 后，确认旧画面的头顶灰白不是 Base Color 错误：Albedo 隔离图保持红色，Beauty 中朝上发束才变灰，来源是 Hair HN 对环境镜面与直接光的过强响应。Hair KK 隔离图同时几乎全黑，说明旧公式的 ramp 阈值、Diffuse 和 Shadow Visibility 连乘在远景丢失了完整高光。

当前约束如下：

- Hair 是非金属介质，环境镜面能量限制为普通材质的 14%，并以 Base Color 保持色相，顶部不得再被天空洗成整片灰白。
- `_HN` 的 RG 基础法线只以 14% 混入常规 Normal；BA 继续驱动双层 Kajiya-Kay 高光。
- KK Power 映射到 80–220，主/次高光直接经过窄 ramp，且不再乘直接光强度或 Shadow Visibility；远景仍需保留一条可读高光。
- Hair AO 是独立风格化体积层。即使 Unreal 实例的 `AO_Color.a` 为 0，也使用其 RGB 和 Style Mask/掠射关系提供保守的发束遮蔽。
- Silhouette outline 只做法线外扩。旧 shader 的裁剪空间 `xy *= 1.6` 已删除，几何宽度降为原值的 58%；禁止再次用整体裁剪空间缩放制造描边。

固定验收至少包含 `face-front` Beauty、`full-body-front` Beauty、Albedo 和 Hair KK。Albedo 用于排除底色错误，Hair KK 用于防止“公式存在但输出全黑”的回归。

## Evening Sky 环境照明修正

此前 Environment 资源虽然成功加载，但 `Endfield Industrial` 背景只混合 8% Skybox，同时负曝光、高对比和较低环境漫反射共同让画面看起来仍是暗色程序背景。现在该 Look 使用 82% Evening Sky 背景，环境纹理按世界法线提供有方向性的漫反射，Toon 阴影区保留 64%–98% 的环境可见度。Look 调色改为 `+0.12 EV / 0.96 saturation / 1.04 contrast`，用于恢复暗色服装细节；这不是全局无差别提亮，主光、环境方向、AO 和阴影染色仍负责明暗关系。

角色材质采用分类稳定规则：Skin/Face/Hair/Fabric/Eye 为 dielectric，packed 纹理中的异常 metallic 值会被分类上限钳制；只有 Metal 类保留完整金属响应。Skin 与 Face 另有 roughness 下限和镜面能量限制，避免肩部、胸口和面部出现白色塑料反光。Face SDF 使用头部局部 X 轴判断左右光照，并以柔和辅助权重参与 ramp；Hair KK 继续由 Hair class、`hair-anisotropy` 特征位、Hair Data 和参数强度共同启用；AO 由材质 `aoColor`、阴影区和 style mask 驱动，Face/Skin 使用较低权重，服装与头发保留完整权重。

人工验收必须同时查看 beauty、albedo、hair-kk、shadow-tint 和 face-sdf 隔离图。beauty 中皮肤高光异常不能用修改 Base Color 掩盖，albedo 用于区分纹理亮斑与照明镜面。

> 适用版本：RenderSettings v6 / Showcase Look v1

## 数据边界

五套角色展示外观位于 `assets_public/showcase_looks.json`。每套外观只负责调色、Bloom 和轮廓参数；背景、地台、Face SDF、材质分类与模型资源仍由各自模块负责。应用启动时验证 schema 和条目数量，格式不兼容会直接报错，不静默回退到 C++ 常量。

| Look | 用途 |
| --- | --- |
| Azure Gallery | 默认公共展示与回归 |
| Endfield Industrial | 低饱和、偏冷的本地角色展示 |
| Neutral Material Check | 材质检查，压低 Bloom 与轮廓干扰 |
| Specular Rim | 高光和边缘光检查 |
| Rear Emissive | 背面与自发光细节检查 |

编辑器 Inspector 可切换 Look，并独立开关 Background、Showcase Platform 和 Face SDF。命令行 QA preset 先应用完整 Look，显式 `--no-stylized` 与 `--no-inner-outline` 的优先级更高。

## 固定验收视角

角色交付至少覆盖全身正面、四分之三、脸部近景、背面、Neutral Material Check，以及 Stylized 开/关对照。文件名采用 `character_<view>_<look>_v<version>_<resolution>.png`，不得使用任务号或临时目录名。

公共 `test_model.gltf` 是无授权风险的自动化资产，两个材质均声明 AzureRender Material Profile v1。它用于 CI、安装包和像素回归，不代表最终角色美术。`assets_private/` 中的角色仅用于本机视觉检查，其模型、纹理、截图和视频不得提交、进入 CI、发布包或公开作品集。

## 修改流程

1. 修改 JSON 后运行 SceneModel tests，确认 schema、条目数量和数值可加载。
2. 运行 `tools/validate_material_profiles.py assets_public/test_model.gltf`。
3. 对公共资产生成 Material Check 与 Stylized A/B，并和基线做容差像素比较。
4. 私有角色只做补充本地 QA，不能代替公共回归。
5. 同步 capture manifest、展示文档和 SHA-256 清单。
