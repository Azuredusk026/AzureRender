# 捕获布局回归

日期为 2026-11-12。
GPU 捕获用例校验材质布局版本 22 和 800 字节相机常量布局。
材质布局以 `PackedMaterialSurfaceProfile` 为准，相机布局以 `UniformBufferObject` 为准。
轮廓光用例同时核对灯光缓冲布局版本 3。

## 测试范围

十三份 GPU 测试脚本共用捕获布局预期。
像素断言、渲染设置、验证层及卸载检查均按所属用例执行。
覆盖测试调用的头发接收夹具通过语法树核对，沿用已经取得的结果。

## 复验记录

Debug 首轮有三项多采样轮廓光用例在捕获清单布局检查处中止。
Debug 和 Release 分别复验四项多采样轮廓光用例，覆盖基础效果、调制、物理材质及视角响应。
两种配置共通过八项，无跳过。
两种配置分别核对 1283 和 1638 份构建产物摘要。
十三份用户保护文件通过摘要核对。

[结果清单](evidence/2026-11-12/capture-layout-regression/results.json)
和[验证归档](evidence/2026-11-12/capture-layout-regression/validation.zip)记录首轮结果、复验结果及产物摘要。
