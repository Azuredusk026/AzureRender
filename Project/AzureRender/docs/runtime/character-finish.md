# 角色材质定稿与验收

角色运行时使用 glTF 2.0 或 GLB。主角原始模型、派生模型与捕获媒体保存在本机私有目录。公共夹具由项目自行制作，包含面部、头发、眉毛和通用实体。

## 素材准备

眉毛卡片通过标准 `material.doubleSided` 设置剔除规则。带 `brow-overlay` 特征的材质可以由准备工具生成双面候选。工具保留原始输入、网格与纹理二进制块，并在派生文件记录来源哈希和材质索引。

```powershell
python tools/prepare_character.py --input "<原始 GLB>" `
  --output "<派生 GLB>" --brow-double-sided
```

输出使用独立路径。路径已有文件时，工具报告错误。材质准入通过 `validate_material_profiles.py` 与 `audit_face_sdf_compatibility.py` 检查。

眉毛区域由骨骼名称与有效权重选择，再按三角形连接关系划分小岛。厚度在绑定空间按小岛中心扩展，垂直扩展上限为 0.0012 米，水平扩展为该值的 0.45 倍。睫毛保留自身几何。Compute 和顶点蒙皮使用同一份区域数据。

`--qa-isolation brow-mask` 输出可见眉毛遮罩。眉毛显示为白色，实体深度保留真实遮挡。遮罩只包含具有眉毛权重的区域。

## 固定输入与相机

验收先复制二进制、运行库和着色器，形成独立捕获目录。`qa-inputs.json` 记录源码、编译产物和工具链缓存哈希。每轮验收记录素材、二进制和着色器哈希。

```powershell
python tools/freeze_qa_runtime.py `
  --executable build/ninja-msvc-release/AzureRender.exe `
  --output build/qa/runtime
```

相机包含全身正面、脸部正面、左右三分之四、左右侧面与背面细节。脸部相机名称为 `face-front`、`face-three-quarter-left`、`face-three-quarter-right`、`face-side-left`、`face-side-right`。`face-three-quarter` 对应左三分之四视角。

眉毛标注采用独立冻结的投影遮罩，保存图像与哈希。标注需要复核左右形状及头发遮挡。脸部标注使用固定相机下的区域图，真实投影边界另行记录。私有角色的标注文件随本机验收目录保存。

## 像素与光照检查

`test_character_finish.py` 捕获七视角与三种光照。每个组合包含 Beauty、眉毛遮罩、关闭 Overlay、CPU 蒙皮及三帧静止序列。它检查标注区域至少 90% 的可见覆盖与效果贡献，并记录 CPU 和 Compute 的图像差异。

```powershell
python tools/test_character_finish.py `
  --executable build/qa/runtime/AzureRender.exe --asset "<派生 GLB>" `
  --output build/qa/finish --annotations "<标注 JSON>" --light-scans
```

标注 JSON 将相机名称映射到眉毛遮罩 PNG。脸部区域键为 `face-front_face` 与 `face-three-quarter-left_face`，路径相对 JSON 所在目录解析。图像尺寸为 1280×720，白色像素表示验收区域。

`--qa-light-scan` 在 120 帧内旋转主光。阴影、主材质与头部局部光照使用相同方向。正面和左三分之四视角分别检查相邻帧平均颜色与加权亮度变化，门禁为 3/255。

`--qa-animation` 在固定相机下播放骨骼动画。`run_character_qa.ps1 -Mode animation` 捕获正面与左右三分之四的动画序列。静止材质检查保持绑定姿态，动作检查需要包含动画片段的资产。

Face SDF 的左右混合窗口为横向分量的 ±0.60，距离阈值的柔化宽度至少为 0.18。角度与距离两处连续混合共同控制明暗过渡。材质的遮罩、暖色阴影和实时投影继续参与合成。

## 公共夹具与混合场景

`assets_public/third_person/material_fixture.gltf` 包含曲面头发、面部、独立眉毛区域和通用躯干。纹理通过明确的 Face SDF 与 HN 数据入口传递。生成入口为 `create_material_fixture.py`。

`AzureEngine.MaterialFixtureGpu` 在实际 Vulkan 上检查材质类别、眉毛贡献、头发高光和面部 SDF。测试还让头部关节运动，检查眉毛跟随与 CPU、Compute 一致性。

`assets_public/scenes/material_mixed.azscene` 包含角色、地面、墙体和物体。场景资源通过运行时资源定位器解析，公共路径相对公共素材根目录。编辑存档保留资源引用，渲染快照携带解析后的路径。

关卡与场景文档采用素材原始米制坐标，实体变换决定模型的位置和缩放。角色与通用物体使用同一世界空间。单资产预览按模型包围盒居中并适配展示尺寸。

GPU 工具隐藏启动窗口，记录超时、退出码、Validation 信息和卸载后的资源数量。图像与日志支持人工复核，阶段验收记录列出实测范围及结果。
