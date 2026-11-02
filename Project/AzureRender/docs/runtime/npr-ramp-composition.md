# NPR 色带颜色合成

材质画像版本二通过 `nprDiffuse.rampComposition` 选择颜色合成。
该字段适用于具有漫反射色带的 NPR 表面。
具体图像、材质身份和比例参数归属于项目内容。

## 配置

```json
{
  "nprDiffuse": {
    "schemaVersion": 1,
    "shadowScale": 0.275,
    "shadowAlbedo": "base-color",
    "lightingInputs": "host-scene-radiance",
    "shadowMask": "one",
    "baseAo": "albedo-alpha",
    "characterVisibility": "object-shadow",
    "rampComposition": "view-ramp-alpha"
  }
}
```

该对象放在完整画像的 `azureRenderMaterial` 内。
画像同时声明 `diffuse-ramp` 特性和 `diffuseRamp` 纹理。
色带采用单行 RGBA 图像、线性颜色及零级采样。
完整文档、参数覆盖和材质图共享相同语义。

| 值 | 颜色合成 |
| --- | --- |
| `mask-visibility` | 使用遮蔽量及主光色带透明度 |
| `view-ramp-alpha` | 使用视角色带加权的遮蔽量 |

缺省值为 `mask-visibility`。
视角策略支持通用、皮肤、织物及金属的 NPR 表面。
加载器校验类型、枚举、画像版本及所属特性。
画像打包也校验直接构造的有类型输入。

## 权重

视角色带沿表面法线与相机前向的夹角采样。
视角权重为该色带透明度乘以原始遮蔽量。
阴影混合量由视角权重加主光色带透明度得到。
该混合量限制在零至一之间。

室内颜色使用视角权重混合阴影与室内亮部。
户外颜色继续消费主光色带及亮部遮罩。
室内与户外颜色按场景可见量合成。
`shadowScale` 控制阴影反照率的整体比例。

公共响应中的遮蔽量保存原始值。
镜面及头发消费者按各自契约施加一次视角权重。
颜色合成独立持有视角加权后的室内权重。
方向光、对象阴影及场景接收组沿用所属配置。

## 程序与布局

GPU 画像布局版本为二十一，大小为 352 字节。
NPR 画像第二行的第四分量保存颜色合成策略。
显式视角策略使用 `_lighting` 着色程序族。
主视图与颜色快照按同一材质条件选择程序。

场景渲染器 API 版本为九。
能力字段 `supportsNprRampComposition` 用于支持查询。
角色渲染器声明支持该能力。
采集清单通过 `nprRampCompositions` 记录显式视角策略。

## 验证

`AzureEngine.NprRampCompositionGpu` 通过生产入口采集。
数值控制覆盖两种材质及两条描述符路径。
场景可见量覆盖零、二分之一和一。
完整文档、透明混合、裁剪和多重采样分别检查。

公共线性误差预算为 0.008。
缺省配置与显式缺省配置要求逐像素一致。
原生来源与范围见[合成核验](../acceptance/nr/npr-view-ramp-composition.md)。
整体画质与交付目标见[开发总计划](../plans/azure-engine-plan.md)。
