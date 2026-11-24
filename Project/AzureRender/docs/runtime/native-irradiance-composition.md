# 原生方向场组合

原生转换入口为 `tools/endfield_native/irradiance_composition.py`。
工具组合显式提供的三层纹理采样和默认球谐系数。
输出为世界空间的三色四维方向系数。
原生运行参数及纹理过滤状态由调用方提供。

## 数据入口

`decode_native_irradiance_sample(raw)` 接受十六字节记录。
幅度解码复用原生读取器。
返回值完整保留方向字节、归一化四元组及 Alpha。
该结果可直接作为一层精确格点输入。

过滤后的纹理结果显式提供幅度和方向四元组。
附带原始字节或 Alpha 元数据时，值须完全一致。
幅度须非负，方向四元组各分量须在零至一之间。

## 组合入口

`compose_native_irradiance(parameters)` 接受版本一映射。
全部字段必须显式提供，未知字段触发错误。
禁用状态也执行完整结构与数值校验。
非法版本、类型、范围及溢出统一抛出 `ValueError`。

| 字段 | 含义 |
| --- | --- |
| version | 整数 1 |
| enabled | 体积启用布尔值 |
| worldPosition | 原生抖动后的三维世界位置 |
| cameraPosition | 版本一的三维中心偏移向量 |
| chunkOrigin | 三维分块原点 |
| centerScales | 三层中心偏移比例 |
| sampledLayers | 三层幅度及三组方向四元组 |
| defaultCoefficients | 三色默认球谐四元组 |

层中心按 `chunkOrigin-cameraPosition*scale` 计算。
3534 的调用向量采用逆观察矩阵 Z 轴。
相机位置与该轴分别记录来源及原始常量。
各层采用已确认的水平与垂直边界连续过渡。
方向图第零组 Alpha 控制默认环境混合。
三色方向系数保留符号，法线求值属于消费端。

返回值包含 `coefficients` 和 `layerWeights`。
`clipmapFades`、`volumeActive` 和 `defaultMix` 提供诊断。
`directionalAlpha` 完整保留各层三组 Alpha。
返回数组与输入独立持有。

## 来源与运行边界

公式来自已保存的延迟光照片元程序。
来源身份及指令范围写入生产模块和专项收据。
一千组外部汇编解释结果与生产入口一致。
源记录、层级和默认环境分别具有独立验证范围。

3534 的默认常量及采样器见[体积输入](../acceptance/nr/volume-image-upload.md)。
设备采样精度与实时抖动按所属运行门禁验收。
现行 `AZIV0001` 保存规则网格的候选方向场。
完整原生运行资产按对应版本另行验收。

执行 `python -m unittest discover -s tools -p 'test_endfield_native*.py'`。
结果及剩余范围见[组合验收](../acceptance/nr/irradiance-composition.md)。
