# Character Look 与美术验收

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
