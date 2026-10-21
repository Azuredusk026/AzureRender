# 原生灯光作用体积

日期：2026-10-21。
阶段索引为 NR3。
状态：公共能力验证通过，原生画面等价待验收。
运行契约见[灯光作用体积](../../runtime/light-influence-boxes.md)。

## 原生静态证据

`HGSharedLightData` 提供盒位置、半尺寸及三轴角度。
字段偏移分别为 100、112 和 124。
阈值位于偏移 136，开关位于偏移 92。
非有限字段由离线读取器拒绝。

`LightCulling.PrepareCPUData` 构建盒的逆变换。
静态入口为 `GameAssembly.dll:0x18c12f5e0`。
位置、半尺寸及角度分别进入 `Matrix4x4.TRS`。
随后调用 `get_inverse` 并打包十二个半精度分量。

角度转换与 `Quaternion.Euler` 的路径一致。
原生 `Internal_FromEulerRad_Injected` 使用顺序值四。
同版本元数据的 `OrderZXY` 值为四。
转换器据此按 Z、X、Y 顺序应用角度。

`HGRP/DeferredLighting` 在逐像素照明前计算遮罩。
表面世界位置先减去灯光世界位置。
三个逆变换行生成盒坐标及边缘权重。
源盒位置和角度表达相对世界坐标。

转换器保留半精度打包误差。
灯光世界位置折入引擎矩阵的平移项。
未知激活状态及实时覆盖保持待核验。
静态恢复结果用于配置候选的来源证明。

## 公共验证

公开夹具使用独立的平面与方向光。
四组画面覆盖完整光源、轴向、旋转和平移盒。
相同夹具分别进入光栅及路径追踪。
路径追踪每组累计 64 采样。

光栅最大权重绝对误差为 0.001885。
路径追踪最大权重绝对误差为 0.010944。
固定阈值为 0.02。
缺失能力的原始夹具最大误差为一。

运行期间 Player 指纹保持一致。
各组通过 Vulkan、运行错误及资源释放检查。
CPU 回归覆盖非法变换、硬边界及 GPU 参数。
场景文档覆盖当前版本与版本六、八的兼容加载。
16 项受影响 CTest 回归全部通过。
八组既有灯光画面及主光阴影夹具通过。

原生导入工具集包含 99 项通过的测试。
读取结果含 229 个启用盒，半尺寸均为正值。
其中 147 个盒保存非零角度。
此计数覆盖场景静态目录，生效灯组另行核验。

## 中庭接入

完整中庭候选的 45 盏局部灯中有 44 个启用盒。
源逆变换、打包精度及阈值均已应用。
两帧 640×360 采样通过运行及资源释放检查。
主方向光、GI 和局部反射保持候选配置。

第二帧平均通道值为 103.3526。
相对缺省作用体积的平均绝对差为 1.7895 字节。
画面可见局部受光范围及亮度变化。
完整原生激活、灯层与视觉误差继续验收。

## 本机证据

```text
build/runs/20261021-125515-native-scene-lighting-5e36f1/
```

`obb-decode-red.log` 保存字段缺失的失败结果。
`obb-converter-red.log` 保存转换能力缺失的失败结果。
`native-ecs-lights-with-obb.json` 保存完整解码字段。
`native-obb-fields.json` 保存盒字段与源摘要。

`light-obb-producer-code.json` 保存生产者指令与来源。
`obb-call-identities.json` 对应调用地址和托管名称。
`obb-euler-native-code.json` 保存原生旋转入口。
`obb-rotation-order.json` 保存角度顺序的元数据常量。

`light-box-red/metrics.json` 保存缺失能力的误差。
`light-box-green/metrics.json` 保存光栅测量。
`light-box-tracing/metrics.json` 保存路径追踪测量。
`light-box-native-tests.log` 保存导入工具回归。
`light-box-ctest-final.log` 保存 16 项受影响回归。
`light-box-regression/metrics.json` 保存既有照明测量。
`light-box-shadow-regression/metrics.json` 保存主光阴影测量。
`native-light-boxes-application.json` 保存真实灯组接入。
`native-light-boxes-comparison.json` 保存实际画面差异。
