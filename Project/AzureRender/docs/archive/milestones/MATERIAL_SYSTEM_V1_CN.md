# AzureRender Material Class / Data v1

## 1. 目标与边界

CQ-1 把“某张贴图刚好带有某个参数”升级为可审计的材质所有权系统。它只负责分类、参数路由、Fallback、诊断和证据，不在本节点实现最终 Toon Ramp、Face SDF、Hair KK 或 Bloom。

配置存放在 glTF Material 的 `extras.azureRenderMaterial` 中。GLB 的 JSON Chunk 本身就是随资产携带的配置，Schema 位于 `schemas/azure_render_material.schema.json`。

## 2. Material Class v1

| ID | Class | 用途 |
|---:|---|---|
| 0 | Generic | 未知或公共资产的中性 Fallback。 |
| 1 | Skin | 身体皮肤及与皮肤共图集的区域。 |
| 2 | Face | 面部主材质，后续 Face SDF 的唯一主要入口。 |
| 3 | Hair | 头发主材质，Hair Anisotropy/KK 的所有者。 |
| 4 | Fabric | 衣装与复合布料金属图集的主分类。 |
| 5 | Metal | 独立金属 Primitive；莱万汀当前没有独立 Metal Primitive。 |
| 6 | Eye | Iris/眼球相关材质。 |
| 7 | Overlay | Brow、Eye Shadow、Hair Shadow 等透明覆盖层。 |
| 8 | Emissive | 独立发光 Primitive；复合衣装发光仍由 Feature Flag 控制。 |
| 9 | Showcase | Renderer 生成的展示地台。 |

主 Class 不假装解决复合纹理。莱万汀的机械、布料和发光区域共存在 Cloth 图集中，因此 Cloth 保持 `fabric`，再由 Metallic/Roughness、Style Mask 和 `emissive-mask` Feature 分区。

## 3. Feature Flags

| Bit | JSON 名称 | 所有权 |
|---:|---|---|
| `0x01` | `stylized-shadow` | 允许材质 Shadow Tint 参数参与。 |
| `0x02` | `hair-anisotropy` | 允许 Hair Data 与 KK 高光参与。 |
| `0x04` | `face-sdf-eligible` | 预留给 CQ-3 Face SDF。 |
| `0x08` | `emissive-mask` | 允许 Emissive Texture/Strength 参与。 |
| `0x10` | `overlay` | 标识透明面部/头发覆盖层。 |
| `0x20` | `neutral-fallback` | 明确表示未知资产采用中性路径。 |

Shader 不再只根据“贴图不为零”启用 Hair、Face Matcap、Emissive 或 Shadow Tint，而会同时检查对应 Feature Flag。

## 4. 参数 ABI

每个 Material Profile 包含两个四维参数：

- `styleParameters = [toon, shadowTint, specular, rim]`；
- `featureParameters = [outline, hairHighlight, emissive, faceOverlay]`。

这些参数连同 Class、Flags 和 Schema Version 通过每次 Draw 的 128-byte Push Constant 传入 Fragment Shader。128 bytes 等于 Vulkan 规范保证的最小 Push Constant 容量，后续若继续扩展必须迁移到 UBO/SSBO，不得继续增大。

## 5. 莱万汀实际审计

| 源材质 | Class | Flags |
|---|---|---|
| `M_actor_laevat_body_01/02` | Skin | Stylized Shadow |
| `M_actor_laevat_face_01` | Face | Stylized Shadow, Face SDF Eligible |
| `M_actor_laevat_hair_01` | Hair | Stylized Shadow, Hair Anisotropy |
| `M_actor_laevat_cloth_01/02` | Fabric | Stylized Shadow, Emissive Mask |
| `M_actor_laevat_cloth_03/04/05` | Fabric | Stylized Shadow |
| `M_actor_laevat_iris_01` | Eye | Stylized Shadow |
| `M_actor_laevat_brow_01` | Overlay | Overlay |
| `M_eyeshadow_common_01` | Overlay | Overlay |
| `M_hairshadow_common_01_001` | Overlay | Overlay |

源 GLB 包含 13 个材质和 13 个角色 Primitive。运行时增加 `FallbackMaterial` 与 `AzureRender_ShowcasePlatform`，所以日志显示 15 个材质和 14 个 Primitive。

## 6. Fallback 与兼容

- 显式 Profile：`source=asset-extras`，Schema Version 必须为 1；未知 Class、Feature 或错误字段类型会使加载明确失败。
- 旧私有资产：按材质名称推断 Class，日志和 Manifest 标记 `source=fallback/inferred`。
- 未知公共资产：统一进入 Generic + Neutral Fallback，不使用莱万汀专用参数。
- 导出工具 `tools/inject_gltf_textures.js` 会生成 13 个显式 Profile；私有派生验证资产为 `assets_private/laevat_skinned/laevat_skinned_material_cq1_v2.glb`，不得进入公开包。
- `tools/validate_material_profiles.py` 只使用 Python 标准库，按同一 Schema 的字段、枚举、唯一性和参数范围验证 glTF/GLB。

## 7. Debug 与证据

- `--qa-isolation material-id` 输出稳定的 Class 调色板；
- 启用 HUD 后显示各 Class 数量，以及 Face/Hair 的 Toon、Specular、Rim 参数；
- Capture Manifest 的 `materialInventory` 保存名称、Class、Flags、两组参数、Profile Source 和 Primitive Count；
- `qaStateHash` 已包含完整材质 Profile，任何参数修改都会改变状态哈希。

CQ-1 验证目录：

- `captures/cq1_laevat_isolation_v1`：13 个 Isolation Case；
- `captures/cq1_material_id_face_v2`：脸部近景 Material ID；
- `captures/cq1_material_hud_v1`：Class/参数 HUD；
- `captures/cq1_public_fallback_v1`：公共资产 Generic Fallback；
- `captures/cq1_release_beauty_v1`：Release 代表 Beauty。

## 8. CQ-1 Exit Gate

- 13 个源材质全部获得显式 Profile；
- Face、Hair、Skin、Fabric、Eye、Overlay 使用不同可解释参数；Metal/Emissive/Generic/Showcase 有完整默认配置；
- Hair、Face、Emissive 与 Shadow 参数只能作用于持有对应 Flag 的材质；
- 公共未知资产使用中性 Fallback；
- Material ID、HUD、Manifest 可以诊断分类与参数；
- Debug/Release 构建、公共 Smoke、私有 13-view Isolation 与代表 Beauty 通过。

CQ-1 完成后，CQ-2 只负责使用这些分类和参数接入真正 Ramp LUT 与阴影层级，不重新设计材质所有权。
