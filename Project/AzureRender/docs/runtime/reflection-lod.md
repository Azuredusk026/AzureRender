# 反射层级映射

局部探针通过 `lodMapping` 声明预滤波层级规则。
该配置服务于导入环境与独立材质预览。
图像、采样器及释放继续归属现有探针纹理对象。

## 配置与版本

探针版本三要求显式的 `lodMapping` 对象。
映射对象使用版本一，`kind` 为 `linear` 或 `logarithmic`。
线性对象只保存版本及种类。
对数对象同时保存三个有限参数。

| 参数 | 范围 | 含义 |
| --- | --- | --- |
| `scale` | 0 至 32 | 对数粗糙度的倍率 |
| `bias` | -32 至 32 | 层级偏移 |
| `minimumRoughness` | 大于零且不大于一 | 对数输入下限 |

```json
{
  "version": 3,
  "texture": "assets:/room.azenv",
  "lodMapping": {
    "schemaVersion": 1,
    "kind": "logarithmic",
    "scale": 1.2,
    "bias": 5,
    "minimumRoughness": 0.001
  }
}
```

无显式映射的资源按既有线性规则采样，保存为版本二。
读取入口兼容版本一、二、三。
映射字段须与版本三共同出现。
未知字段、非法数值及解析失败遵循明确拒绝与目标保持规则。

## 求值与上传

有限粗糙度先限制到零至一。
线性层级为粗糙度乘以最后层级。
对数层级按以下公式计算，并限制到图像的实际范围。

```text
lod = clamp(scale * log2(max(roughness, minimumRoughness)) + bias, 0, lastMip)
```

CPU 的 `ReflectionProbe::lod` 接收粗糙度及层级数量。
层级数量范围为一至三十二。
GLSL 主通道和快照使用同一求值函数。
各像素使用经过材质与法线足迹处理的有效粗糙度。

GPU 参数复用三个空间行的第四分量。
它们依次为倍率、偏移和最小粗糙度。
零最小值表示既有线性规则，正值选择对数规则。
相机缓冲布局、纹理绑定及着色器变体规模保持既定契约。

参数范围与 GPU 检查见[层级验收](../acceptance/nr/reflection-lod.md)。
