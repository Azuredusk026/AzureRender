# 辐照接收位置

辐照资源消费世界空间的查询位置。
`IrradianceQueryParameters` 定义位置偏移和像素抖动。
CPU 与 GLSL 共享精确常量及融合乘加顺序。
物理材质主颜色和反射快照消费同一查询函数。

## 参数

| 字段 | 默认值 | 规则 |
| --- | --- | --- |
| `normalOffset` | 0 | 有限非负数，沿查询法线偏移的距离 |
| `jitterAmplitude` | 0 | 有限非负数，沿所选坐标轴叠加的抖动幅度 |
| `phase` | 0 | 有限数，绝对值最多为 16,777,216 |
| `pixelOrigin` | `top-left` | 噪声采用左上或左下原点 |
| `jitterSpace` | `world` | 抖动沿世界轴或辐照场坐标轴叠加 |

查询先沿单位世界法线偏移，再叠加同一标量抖动。
输入像素采用当前视口的左上原点，坐标向下取整。
左下噪声原点按 `height - 1 - floor(y)` 转换。
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

方向策略使用 `IrradianceQueryFrame` 提供视口高度和场方向。
`worldToField` 保存三行正交轴，支持旋转及镜像。
字段空间的方向为该矩阵转置乘以 `(1,1,1)`。
方向保持原有长度，位置平移不参与方向变换。

左下原点要求有效的视口高度。
正交轴的单位长度及垂直误差至多为 `1e-5`。
规则体积的场方向使用世界轴。
三层辐照场从既有资源矩阵读取方向。

## 场景绑定

`irradianceVolume` 的版本二保存默认方向的采样参数。
版本一读取时提供零偏移、零幅度和零相位。
方向策略使用版本三，明确保存两个策略字段。
场景文档与关卡保存同一公共绑定对象。
资源载荷、纹理格式及描述符布局由资源类型决定。

```json
{
  "irradianceVolume": {
    "version": 3,
    "asset": "assets:/field.bin",
    "intensity": 1,
    "enabled": true,
    "sampling": {
      "normalOffset": 0.25,
      "jitterAmplitude": 0.2,
      "phase": 49,
      "pixelOrigin": "bottom-left",
      "jitterSpace": "field"
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
启用分量保存启用位、原点位及方向位，数值范围为零至七。
采样使用现有视口高度及场矩阵，GPU 布局保持现行版本。
同一路径的采样调整允许复用场景资源。
运行报告的 `irradianceQuery` 保存实际参数。
`irradianceQueryScope` 标记物理表面的消费范围。
诊断采用单精度往返精度，保留整数边界附近的相位。
来源及设备范围见[查询验收](../acceptance/nr/irradiance-query.md)。
像素原点与场方向见[坐标验收](../acceptance/nr/irradiance-query-frame.md)。
