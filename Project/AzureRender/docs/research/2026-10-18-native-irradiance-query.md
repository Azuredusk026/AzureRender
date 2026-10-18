# 原生辐照体积的空间查询

此能力属于 NR4 原生 GI 调查。
工具读取指定数据文件与层级的源格点。
输出保留源字节地址、幅值及方向图数据。
工具支持单采样的方向求值。
整体场景组合与运行时分支激活仍待验证。

## 来源与证据

来源为本机游戏的 `IrradianceVolumeUpdateV3CS`。
资源 PathID 为 `5362930397531259258`。
资源 SHA-256 为 `a31b543458e5abb38dee40d4a000ea1e27cafe6ffe23aeb32ce6f415ddb96722`。
本次使用已导出的 DXBC 静态反汇编。

证据根目录为 `build/runs/20261017-151417-native-render-restoration-e7723b/`。
字节码目录为 `gi-v3-actual-shader-bytecode/`。
资源定位记录见 `gi-v3-actual-shader-provider.json`。
变体关键词与字节码摘要见该目录的 `manifest.json`。

| 证据 | 已观察行为 |
| --- | --- |
| `FullUpdateClipmap0`，变体 8 | 16 字节采样与三组方向纹理写入 |
| `FullUpdateClipmap1/2` | 层级对应的位移与空间索引 |
| `PartialUpdateClipmap0/1/2` | 局部更新使用同类空间索引 |
| `FullUpdateClipmap0`，变体 0 | 低质量采样步长为 5 字节 |

## 空间地址

[irradiance.py](../../tools/endfield_native/irradiance.py) 提供 `world_address`。
输入为游戏世界坐标 `p` 与层级 `L`。
层级范围为 0 至 2。
世界坐标各轴的绝对值须小于 2048。

各轴按下式计算：

```text
q = floor(2*p + 0.5) + 4096
outer = (q >> (6+L)) << L
brick = (q >> (2+2*L)) & ((16 >> L)-1)
local = (q >> (2*L)) & 3
```

索引记录坐标为 `[outer.x, outer.y, outer.z, L]`。
占用位采用六位 Morton 顺序。
两组位分别依次排列 `x、y、z`。
空间采样步长分别为 0.5、2、8。

`query_world` 要求显式选择 `file_index`。
结果记录 `tile_index`、占用位与文件字节偏移。
缺失格点返回 `sample: null`。
调用方负责插值、层级回退与区域覆盖。

## 方向图数据

高质量变体将后 12 字节拆成三组 RGBA。
各分量除以 255，写入方向图的三段纵向区域。
`directional_atlas_unorm` 输出这三组线性数值。
`amplitude` 输出首四字节的压缩 RGB 幅值。

延迟着色的片元指令建立了直接方向消费公式。
证据为 `deferred-lighting-bytecode/segment1-1001480.asm`。
对应 DXBC 摘要为 `32a764cb2a1a9210ee1a4104dccfdf67e91a2452e42e834a73bee98115d21c37`。
源资源 PathID 为 `6850169740889141214`。

| 片元槽位 | 指令中的用途 |
| --- | --- |
| `t24`、`t25` | 第 0 层幅值与方向图 |
| `t26`、`t27` | 第 1 层幅值与方向图 |
| `t28`、`t29` | 第 2 层幅值与方向图 |

第 2310 至 2411 行读取三层纹理。
各方向组的 RGB 按 `4*u-2` 转换。
各组对应红、绿、蓝三个幅值通道。
线性系数乘幅值，常量项直接取幅值。

`decode_directional_coefficients` 输出每色四个系数。
顺序为世界空间 `x、y、z、常量`。
方向组的 Alpha 保留为独立数据。
第 0 组 Alpha 参与默认环境项的混合控制。

第 2520 至 2531 行执行下式：

```text
coefficient[c] = amplitude[c] * [4*u[c].xyz-2, 1]
rgb[c] = max(coefficient[c].w
             + dot(coefficient[c].xyz*strength[c], normal), 0)
```

`evaluate_directional_sample` 接收单位世界法线。
调用方可传入每色方向强度。
该函数评价默认环境系数为零的单个源采样。
可选的 `t23` 非线性处理保持关闭。

完整渲染须先合并插值与层级系数，再做最终钳位。
默认环境系数、区域覆盖和场景激活仍待接入。
纹理槽位用途由指令直接确认。
符号属性名与实时描述符绑定仍待独立验证。

## 验证范围

单元测试覆盖三层地址、负数边界及 Morton 位序。
双文件夹具验证显式选择、稀疏空洞与源字节偏移。
命令行回归覆盖完整检查及世界查询入口。
原生工具集的 72 项测试全部通过。

实测查询位置为 `(0, 1.035, 9)`。
两个源文件的第 0 层均缺失该格点。
第 1、2 层各读取到不同幅值的源采样。
记录见 `gi-v3-world-query-evidence.json`。
方向系数及三个轴向结果另有结构化记录。
文件为 `gi-v3-directional-consumer-evidence.json`。

上述结果属于离线读取验证。
NR4 的引擎画面验收仍按正式计划执行。
