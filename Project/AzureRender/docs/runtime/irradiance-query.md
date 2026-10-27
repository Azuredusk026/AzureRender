# 辐照接收位置

辐照资源消费世界空间的查询位置。
`IrradianceQueryParameters` 定义位置偏移和像素抖动。
CPU 与 GLSL 共享精确常量及融合乘加顺序。
物理材质主颜色和反射快照消费同一查询函数。

## 参数

| 字段 | 默认值 | 规则 |
| --- | --- | --- |
| `normalOffset` | 0 | 有限非负数，沿查询法线偏移的距离 |
| `jitterAmplitude` | 0 | 有限非负数，沿三个世界轴叠加的抖动幅度 |
| `phase` | 0 | 有限数，绝对值最多为 16,777,216 |

查询先沿单位世界法线偏移，再叠加同一标量抖动。
像素使用左上角原点，坐标向下取整。
像素坐标须有限、非负且小于 16,777,216。
相位向零截断后进入交错梯度噪声。

CPU 查询拒绝非法参数、非单位法线和非有限输入。
位置计算溢出时报告失败。
零偏移和零幅度返回原表面位置。
体积覆盖、层间权重和默认环境按所得位置计算。

CPU 调用方先求查询位置，再消费对应体积：

```cpp
IrradianceQueryParameters sampling{0.25F, 0.2F, 49.F};
auto position = sampling.queryPosition(surface, normal, pixel);
auto incident = field.query(position).evaluate(normal);
```

## 场景绑定

`irradianceVolume` 的版本二保存采样参数。
版本一读取时提供零偏移、零幅度和零相位。
场景文档与关卡保存相同的版本二对象。
资源载荷、纹理格式及描述符布局由资源类型决定。

```json
{
  "irradianceVolume": {
    "version": 2,
    "asset": "assets:/field.bin",
    "intensity": 1,
    "enabled": true,
    "sampling": {
      "normalOffset": 0.25,
      "jitterAmplitude": 0.2,
      "phase": 49
    }
  }
}
```

`phase` 由内容或调用方提供。
固定相位用于复核对应截帧的查询输入。
连续帧的相位生产与时序质量按所属视图验收。
物理表面应用绑定中的位置偏移与抖动。
NPR 漫反射、投影头发和眼睛使用原表面位置。
这些消费者分别使用对应材质的间接光法线。

参数使用现有相机四维行的三个分量。
同一路径的采样调整允许复用场景资源。
运行报告的 `irradianceQuery` 保存实际参数。
`irradianceQueryScope` 标记物理表面的消费范围。
诊断采用单精度往返精度，保留整数边界附近的相位。
来源及设备范围见[查询验收](../acceptance/nr/irradiance-query.md)。
