# 原生辐照体积读取证据

日期：2026-10-17。
状态：源索引及稀疏载荷通过验证。
世界采样与方向评价继续调查。

## 来源与格式

源版本为终末地 1.0.14。
虚拟目录为 `Data/IrradianceVolume/PC/base01_lv001/v3/`。
读取器为 `tools/endfield_native/irradiance.py`。
输入保持只读，输出记录文件摘要。

索引包含 146 个区块，分属两套场景分支。
完整校验覆盖 25,326 个空间散列键。
载荷共含 1,114,623 个有效采样。
文件范围、占用位及载荷长度均通过一致性检查。

采样包含压缩 RGB 幅度与方向字节。
幅度解码依据原生 V3 区域上传着色器。
空间散列检查同时参考 V2 间接更新着色器。
两者的世界坐标适用关系仍需单独证实。

## 验证范围

七项合成测试覆盖格式版本、截断和索引范围。
同时覆盖空间散列、占用位排序和分支区分。
真实数据读取重放使用以下查询：

```powershell
python tools/endfield_native/irradiance.py build/runs/20261017-151417-native-render-restoration-e7723b/gi-native-data --output build/runs/20261017-151417-native-render-restoration-e7723b/gi-tool-review.json --tile 66 --brick 0 0 1 --bit 0
```

运行根为 `build/runs/20261017-151417-native-render-restoration-e7723b`。
`gi-shader-bytecode/manifest.json` 保存字节码来源。
`gi-tool-review.json` 保存本次真实读取结果。

## 消费边界

世界原点、采样间距及半格偏移仍待证实。
方向字节的基底与评价函数仍待证实。
场景分支选择须与灯组及反射保持一致。
引擎实际间接光消费及画面对照保持待验收。

现有幅度查询用于定位源数据。
原生 GI 等价验收还需方向、插值与遮挡验证。
