# 原生辐照体积的空间查询

此能力属于 NR4 原生 GI 调查。
工具读取指定数据文件与层级的源格点。
输出保留源字节地址、幅值及方向图数据。
最终辐照度求值与运行时分支激活仍待验证。

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

此上传内核没有完成法线相关的辐照度求值。
完整求值仍需定位片元着色器的方向图消费者。
待确认项包括系数重映射、基函数和钳位规则。
场景分支选择及区域混合也需实际消费链证据。

## 验证范围

单元测试覆盖三层地址、负数边界及 Morton 位序。
双文件夹具验证显式选择、稀疏空洞与源字节偏移。
命令行回归覆盖完整检查及世界查询入口。
原生工具集的 70 项测试全部通过。

实测查询位置为 `(0, 1.035, 9)`。
两个源文件的第 0 层均缺失该格点。
第 1、2 层各读取到不同幅值的源采样。
记录见 `gi-v3-world-query-evidence.json`。

上述结果属于离线读取验证。
NR4 的引擎画面验收仍按正式计划执行。
