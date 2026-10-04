# 公共测试资产

`test_model.gltf` 是项目制作的自包含 glTF 2.0 立方体模型，用于验证网格读取、法线、切线生成、纹理上传、PNG 解码、材质 Alpha 模式、双面渲染、多材质描述符、关节动画和 Morph。

模型几何、动画、关节数据、Morph 数据和 2×2 纹理均嵌入文件。单关节线性动画与位置 Morph 目标用于比较 Compute 与顶点着色器回退。

`toon_ramp_profiles.json` 是版本化的角色 Ramp 配置；`toon_ramp_atlas.ppm` 是对应的线性 RGB 采样图集。使用 `python tools/build_toon_ramp_atlas.py --check` 校验图集。

`showcase_looks.json` 保存角色色调、Bloom 和轮廓预设。加载器按配置结构与字段范围校验预设。

`scenes/clustered_lights_empty.azscene` 与 `scenes/clustered_lights_16.azscene` 使用同一角色模型，分别提供零光源参考和 16 个点光源场景。光源实体挂接在隐藏的场景变换节点上。

`scenes/r4_stress.azscene` 包含 32 个独立公共网格资源、32 个可见对象和 16 个点光源，供复杂负载与生命周期验收使用。全部资源引用项目自制的 `test_model.gltf`。

`third_person/material_fixture.gltf` 与 `fixture_hair_data.png` 为项目自制的 MIT 许可材质夹具。模型具有面部、曲面头发、眉毛和通用实体，三个关节用于头部跟随检查。生成入口为 `tools/create_material_fixture.py`。

`scenes/material_mixed.azscene` 放置该角色、地面、墙体和物体。公共 GPU 测试检查材质贡献、混合渲染、动态眉毛以及两种蒙皮路径的一致性。
