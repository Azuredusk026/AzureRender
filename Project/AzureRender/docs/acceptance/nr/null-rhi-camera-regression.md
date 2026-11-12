# NullRhi 录制相机回归

日期为 2026-11-12。
`AzureRender.NullRhiPass` 使用实际角色渲染器和 NullRhi 记录命令。

## 测试相机

常规录制夹具将相机放在目标前方五个场景单位，视线方向有效。
远距背向夹具保留独立的相机位置与朝向。
GPU 裁剪继续检查六个平面的分量均为有限值。

## 双配置复验

两种配置首次执行均报告非有限裁剪平面。
失败来自测试相机的位置与目标重合，原始 CTest 日志随结果归档。
显式设置相机位置后，Debug 和 Release 的 NullRhi 录制检查分别在 10.91 与 2.32 秒内通过。
Release 同组的六项检查也通过。

双配置仅重新生成 NullRhi 录制测试程序。
其余 2,919 份构建产物逐份通过摘要核对。
Debug 仍登记 605 项，Release 登记 607 项。

[结果清单](evidence/2026-11-12/null-rhi-camera-regression/results.json)
与[验证归档](evidence/2026-11-12/null-rhi-camera-regression/validation.zip)保存首次执行、定向重建、CTest 结果及产物摘要。
归档共 35 项，大小为 780887 字节。
