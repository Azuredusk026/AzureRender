# 参考头发半层采样集成

日期为 2026-11-12。
独立集成工作树汇合 `9d5de3d` 与画质提交 `11b88e4`。
受检源码树为 `0d79dcb8b8d15a9e2b1c82afdb33ebb912c8e664`。

## 材质布局与构建

半层采样属于版本二头发材质的参考法线规则。
Release 材质结构为 3928 字节，预备像素偏移为 3720 字节。
Debug 材质结构为 4504 字节，预备像素偏移为 4296 字节。
两种配置分别核对 896 份着色器，两个参考可见量片元程序与画质构建逐字节一致。

## 回归结果

| 配置 | CPU 用例 | 每块显卡的 GPU 用例 | 可玩编辑流程 |
| --- | ---: | ---: | ---: |
| Debug | 13 | 6 | 147.56 秒 |
| Release | 13 | 5 | 26.16 秒 |

两种配置在 NVIDIA 和 Intel 上的五类参考附件及三帧画面与画质分支对应产物逐字节一致。
两种配置的原始四帧均匹配冻结摘要，验证层和卸载检查通过。

完整参考接收超差为 NVIDIA 237 处和 Intel 494 处。
Intel 参考法线、头发可见量和头发 RGB 分别有 25、111 和 568 处超差。
整体状态为 `failed-user-review`。

[采样规则](reference-hair-mip-boundary.md)、[结果清单](evidence/2026-11-12/reference-hair-boundary-integration/results.json)和
[验证归档](evidence/2026-11-12/reference-hair-boundary-integration/validation.zip)保存相关验收依据。
