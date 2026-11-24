# 原生逐灯镜面倍率证据

日期：2026-10-21。
阶段索引为 NR3，状态为公共能力及实景候选通过。
原生激活、专用材质与最终视觉等价继续验收。

## 来源与契约

UnityPlayer 的原生入口位于 `0x1810406a0`。
名称为 `UnityEngine.HGSharedLightData::get_specularIntensity_Injected`。
它通过组件解析器 `0x18021b190` 读取偏移 `0x14` 的浮点数。
源 ECS 组件编号为 63，该偏移对应二十字节。
原始指令、源摘要及解析器证据保存于本机收据。

源目录包含 232 盏去重灯，其中 91 盏的倍率不是一。
当前中庭候选的 45 盏局部灯均按源身份匹配。
其中 26 盏具有非默认倍率。
环境主光使用序列化的 `m_DirectSpecularIntensity=0.195`。

公共配置为 `shape.specularScale`。
默认值、验证规则及生命周期见[运行契约](../../runtime/light-specular-response.md)。
光栅与路径追踪分别缩放直接镜面项。
场景文档版本十保存倍率，版本九兼容测试保留源灯盒。

## 公共验证

主 Debug 配置完成全部目标构建。
原生导入工具的 107 项测试通过。
16 项受影响 CTest 通过，覆盖灯光、路径追踪、场景与编辑保存。
非法负值、无穷值与非数值均有拒绝用例。
路径追踪的倍率上传及历史失效具有独立检查。

两个后端各验证六组画面。
夹具同时覆盖纯金属与具有漫反射的混合表面。
纯金属在零倍率时为黑色，混合表面保留漫反射。
零、0.195 与一三组亮度满足线性组合关系。
图像为八位输出，测量保留每个颜色字节的量化区间。
所有有效像素的观测区间均与预测区间相交。
归一误差阈值为 0.025，量化范围外的最大误差为零。

八组既有灯光画面回归通过。
所有公开运行均无脚本、表现层及 Vulkan 校验错误。
卸载后缓冲与图像计数归零。

## 实景采样

中庭保留原生主光阴影、灯盒、GI 与反射。
两帧 640×360 采样通过运行及释放检查。
第二帧平均颜色通道值为 102.0677。
相对倍率一的基线平均绝对差为 1.3179 字节。
该结果验证源倍率进入实际照明。
原生状态选择、角色着色和近乎原生画面门禁仍待完成。

## 本机证据

证据目录为：
`build/runs/20261021-125515-native-scene-lighting-5e36f1/`。

`specular-native-getter.json` 保存原生读取器指令。
`native-ecs-lights-with-specular.json` 保存源灯光字段。
`specular-red-build.log` 与 `specular-red-python.log` 保存失败用例。
`specular-build-final.log` 保存完整构建结果。
`specular-raster-lab-final/metrics.json` 保存光栅测量。
`specular-tracing-lab-final/metrics.json` 保存路径追踪测量。
`specular-light-regression/metrics.json` 保存既有灯光回归。
`specular-ctest-affected.log` 保存受影响测试结果。
`native-light-response-verification.json` 保存真实场景运行收据。
