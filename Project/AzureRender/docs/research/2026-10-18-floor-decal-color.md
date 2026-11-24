# 地面彩线来源与烘焙颜色契约

调查日期为 2026-10-18。阶段索引为 NR2。
结论基于本机原生实景、纹理及导出数据。
地面的彩色平行线属于原生场景内容。
其亮度、屏幕线宽和反射仍需标定。

## 实景与资产来源

原生截图 `game-reference/atrium-000.jpg` 可见彩线。
角色脚边及画面右下均有彩色道路边线。
截图属于白黄色帝江号中庭状态。
截图相机及曝光尚未完成定量标定。

截图位于运行目录：
`build/runs/20261017-151417-native-render-restoration-e7723b/`。

目标资源为 `mesh-194538cad0927e3e.glb`。
材质为 `M_mdecal_com_zmdpenicon+1_001_04`。
原材质标识如下：
`CAB-44a69a4c67fe5517705ccd8b0e4204cf:9095474107093916738`。

其着色器为 `HGRP/Lit`。
关键字包含 `_ALPHATEST_ON` 和 `_TWO_BASEMAP`。
同时包含 `_ZWRITE_OFF`。
基色图为 `T_rdecal_com_marking_label+1_009_01_D.png`。

该图尺寸为 2048×2048，图底有蓝红绿条带。
不透明条带的代表样本如下。
颜色以 PNG 内的八位 RGB 数值表示。

| 条带 | 原始 PNG 样本 | 透明度 |
| --- | --- | --- |
| 蓝 | 101、135、216 | 255 |
| 红 | 206、85、108 | 255 |
| 绿 | 86、207、146 | 255 |

源图在列 1024 的蓝色主体高 12 像素。
红色主体高 13 像素，绿色主体高 12 像素。
1024 分辨率的贴图具有约六像素条带。
屏幕线宽还受 UV、距离及过滤影响。

## 颜色与透明度契约

Blender 捕获结果使用线性浮点数。
基色和发光的 RGB 显式编码为 sRGB。
输出图像使用直通透明度的 RGBA8 格式。
Alpha、法线和 ORM 数据使用线性数值。

发光按峰值归一化后保存颜色贴图。
峰值由材质的发光强度字段持有。
glTF 的标准 sRGB 消费路径恢复线性颜色。
引擎的 glTF 颜色空间契约保持标准语义。

诊断样本中的蓝色 PNG 值为 33、62、175。
这些数值对应源蓝色的线性量化值。
再次按 sRGB 解码会使结果偏暗。
公开导出测试覆盖 PNG 字节和消费结果。

目标材质的 Alpha 裁剪阈值为 0.731。
彩线主体的源 Alpha 和候选 Alpha 均为 255。
候选使用 `MASK` 模式及相同裁剪阈值。
完整透明区域按已有材质裁剪规则生成。

## 验证记录

公开夹具包含混色、半透明、ORM 和 HDR 发光。
验证读取 GLB 内嵌 PNG 的实际字节。
基色消费后的最大绝对误差为 0.00198。
HDR 发光 2.0 的最大误差为 0.00578。

Alpha 0.4 的消费误差为零。
粗糙度和金属度的误差均小于 0.00197。
法线及 ORM 的线性解释通过 Blender 检查。
非法阈值和非 UV 输入继续受校验。

单材质候选从原始未烘焙场景生成。
三条彩线的代表 PNG 样本与源图一致。
完整重烘焙保留参考场景的资源标识和节点。
场景画面验收由 NR2 联合对照执行。

完整候选包含 60 个资源及 47 个材质。
逐项比对确认全部顶点属性和索引相同。
节点、资源标识及路径均与参考清单一致。
材质透明状态及基色图的 Alpha 像素全部相同。

本轮证据目录为：
`build/runs/20261017-175206-nr-playable-integration-d4e789/`。

| 文件 | 用途 |
| --- | --- |
| `decal-source-material-manifest.json` | 原材质、贴图和关键字 |
| `decal-source-alpha-check.json` | 源图及诊断导出的条带和 Alpha |
| `decal-colour-corrected/material.glb` | 独立单材质候选 |
| `bake-colour-red/exchange-metrics.json` | 已保存的失败夹具 |
| `bake-colour-green/exchange-metrics.json` | 正常 glTF 消费验证 |
| `rebake-source-audit.json` | 原场景的未烘焙状态审计 |
| `colour-corrected-exchange/rebake-manifest.json` | 完整候选及来源摘要 |
| `full-colour-rebake-verification.json` | 几何、节点及透明度逐项比对 |
