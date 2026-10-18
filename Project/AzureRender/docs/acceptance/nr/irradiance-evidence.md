# 原生辐照体积读取证据

日期：2026-10-18。
状态：源查询、方向评价与通用 GPU 消费已验证。
原生组合及实景等价继续验收。

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
幅度解码与世界查询依据实际 V3 上传着色器。
方向系数依据延迟光照片元消费者解码。
绑定链及地址公式见[空间查询](../../research/2026-10-18-native-irradiance-query.md)。

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

世界地址、方向系数及单采样求值已有来源证据。
规则网格导出保留源摘要及有效权重。
通用 GPU 的六组像素对照通过。
最大线性误差为 0.00520，阈值为 0.012。
资源迁移与八组反射回归通过。
契约及证据见[方向辐照网格](../../runtime/irradiance-volumes.md)。

中庭两个分支候选各完成四帧真实采样。
采样保留形状灯、局部反射及原生待机角色。
角色与场景表面均可见间接光贡献。
两次运行的验证层及脚本错误均为零。
进程退出码为零，缓冲与图像释放计数归零。

证据根为 `build/runs/20261017-175206-nr-playable-integration-d4e789/`。
摘要见 `gi-candidate-verification.json`。
拼图为 `gi-candidate-comparison.png`。
各分支保留命令、帧序列、日志与运行报告。

场景分支选择须与灯组及反射保持一致。
原生默认环境项、层级过渡及画面对照保持待验收。

现有幅度查询用于定位源数据。
原生 GI 等价验收还需方向、插值与遮挡验证。
