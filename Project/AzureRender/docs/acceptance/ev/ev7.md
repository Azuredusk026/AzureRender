# 材质语义与元数据验证

日期：2026-10-16。
状态：离线工具通过，GPU 外观验收待集中执行。
范围：现有角色与公共材质夹具。

## 当前能力

工具入口为 `tools/convert_material_semantics.py`。
输入接受 glTF JSON 和 GLB。
输出包含槽位、贴图通道、色彩空间及损失报告。
候选文件通过 `--candidate` 写入独立路径。

`--expected-count 13` 检查角色材质数量。
槽位优先采用显式 `azureSlotId`。
唯一名称采用 `name:` 身份，其他采用索引身份。
重复显式身份和非法纹理引用会被拒绝。

`--restore-map` 从映射表恢复完整材质对象。
恢复包含 Azure 扩展、alpha、因子及标准扩展。
按稳定槽位匹配材质，支持唯一名称的顺序变化。
纹理身份布局改变时使用显式源资产恢复。
`--restore-source` 校验映射记录的源文件指纹。
恢复入口嵌入源图像字节，并保留原贴图索引。
源材质与槽位集合必须匹配，失败时拒绝候选。

普通 PBR 可通过 `--documents-dir` 导出。
输出遵循 `.azurematerial` 版本 1 契约。
纹理身份通过 `--texture-uuids` 引用已注册图片。
专用材质使用保留元数据的 glTF 路径。

## 通道契约

| 语义 | 输入通道 | 色彩空间 | 引擎用途 |
| --- | --- | --- | --- |
| 基色 | baseColorTexture.rgb | sRGB | 基色 |
| 法线 | normalTexture.rgb | 线性 | 切线空间法线 |
| 粗糙度 | metallicRoughnessTexture.g | 线性 | 粗糙度 |
| 金属度 | metallicRoughnessTexture.b | 线性 | 金属度 |
| 打包 AO | metallicRoughnessTexture.r | 线性 | surface-ao 特性下的物理 AO |
| 发光 | emissiveTexture.rgb | sRGB | 发光颜色 |
| glTF 遮蔽 | occlusionTexture.r | 线性 | 当前加载器的风格遮罩 |

glTF 遮蔽输入与物理 AO 具有不同入口。
工具逐材质记录 `occlusion_is_style_mask`。
纹理采样坐标及扩展变化进入复核待项。
通道映射不执行猜测性的纹理重打包。

内置材质文档含四个纹理槽。
脸部 SDF、头发与透明参数通过 glTF 元数据保留。
普通文档导出会拒绝无法完整表达的专用配置。
未注册图片 UUID 也会触发明确失败。

## 实际验证

源资产为 `laevat_repaired_v2.glb`。
已生成 13 槽映射表和独立 GLB 候选。
源材质全部字段保持一致，仅补充稳定槽位身份。
二进制尾块及其他 JSON 字段逐项比较通过。

公共 `material_fixture.gltf` 已生成映射报告。
九个自动测试通过，新增功能先观察失败再实现。
覆盖通道隔离、稳定槽位、元数据恢复和非法引用。
同时覆盖计数、普通文档导出和有损覆盖拒绝。
报告与候选输出也验证了源文件只读约束。

实际 Blender 5.1.2 完成公共夹具往返。
四个材质及两张原贴图恢复通过逐项比较。
角色四方向静止画面与 CPU/GPU 图像检查通过。
材质文档、透明与全量回归由 EV8 汇总。

## 现存待项

原生场景样本依赖 EV1 专用读取器输入。
当前场景映射范围为公共夹具。
原生 shader 身份缺失时明确标记 `unknown`。
工具输出保持 `shaderEquivalenceVerified=false`。

屏幕深度、场景颜色和多 Pass 依赖逐材质登记。
缺少原始 shader 证据时，其状态保持未知。
确定场景及所需效果后，按独立能力验收。
艺术外观由操作者在固定镜头下复核。

本轮证据目录：
`build/runs/20261016-142710-ev7-material-029f01/`。
角色与公共夹具报告分别保存在该目录。
`roundtrip-verification.json` 保存实际比较结果。
候选文件及源文件哈希保存在角色映射报告。

## 画面检查说明

侧面刘海遮挡采用仓库既有外观标注。
未加载标注的初次失败记录保留在证据目录。
前视眉毛贡献率为 99.375%，门槛为 90%。
各镜头 CPU/GPU 差异均满足既有预算。
