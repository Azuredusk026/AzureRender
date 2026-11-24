# 材质文档自发光解释

日期：2026-10-22。范围：NR6-C15 的自发光子项。
公共材质文档通过 `MaterialData::interpret` 生成运行参数。
正值自发光强度启用 `MaterialFeatureEmissiveMask`。
注册类型的其他功能位保持各自语义。

PBR 与 Stylized 均覆盖保存、解析和运行解释。
零强度保留缺省功能位，颜色仍来自自发光纹理。
纹理缺省值为黑色。
材质文档示例包含注册表要求的完整参数。

## 验证范围

新增测试首先在缺少功能位时失败。
失败信息为 `Saved emissive strength must enable the shader emission consumer`。
修复后 Release 的四项材质检查通过。
范围覆盖文档、材质图、材质图编辑及槽位编辑。

运行记录位于以下目录：

```text
build/runs/20261022-230758-main-material-integration-8fd37c/
```

日志为 `material-contract-regression.log`。
此次验证覆盖 CPU 生产解释与编辑回归。
专项 GPU 结果见[覆盖验收](material-parameter-overrides.md)。
完整替换与参数覆盖均与 glTF 自发光输入逐像素一致。
完整角色材质表达及 C15 总体验收保持待项。

## 完整替换诊断

`applyMaterialBindings` 保持完整替换语义及槽位身份。
特殊表面语义丢失时，通过公共诊断输出槽位及字段。
普通等价材质替换保持安静。
此诊断用于加载及编辑预览共用的生产路径。

新增用例首先以缺少诊断失败。
修复后验证 SDF、NPR、色带及槽位身份。
Release 四项材质与编辑检查再次通过。
日志为同目录 `material-replacement-regression.log`。
