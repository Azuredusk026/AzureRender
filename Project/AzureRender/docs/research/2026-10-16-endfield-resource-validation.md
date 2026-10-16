# 终末地资源验证依据

日期：2026-10-16。
状态：静态调查完成，资源读取与运行验证待执行。
范围：莱万汀、一个场景切片、一个动作和表情。

## 本机样本

角色目录为 `assets_private/laevat_skinned/`。
下表来自 GLB 的 JSON 块与文件 SHA-256。
四个样本均有 469 个节点、468 个蒙皮关节。
每份文件包含一个 skin、一个 mesh 和 13 个材质。

| 文件 | 字节数 | 动画 | 形态目标数 |
| --- | ---: | --- | ---: |
| `laevat_repaired_v2.glb` | 82160908 | `Afterglow_ProceduralIdle` | 0 |
| `laevat_g5_source_animated.glb` | 71624012 | `idle`、`walk` | 0 |
| `laevat_g5_animated.glb` | 69671768 | `idle`、`walk` | 0 |
| `laevat_idle_material_face_sdf.glb` | 69406068 | `Afterglow_ProceduralIdle` | 0 |

修复版作为外观基线，带动作版本作为播放基线。
动作来源沿用[第三人称计划](../plans/third-person-playable-plan.md)的重定向记录。
程序待机与重定向动作各自保留来源类别。
游戏原生动作需要另行取得并验收。

修复版包含 `Face_Head`、`jawJoint` 和嘴唇骨骼。
还包含眼睑、眼睫与面颊命名的骨骼候选。
这些名称证明骨架存在相关节点。
实际控制作用、权重覆盖与表情曲线仍待验证。

| 文件 | SHA-256 |
| --- | --- |
| 修复版 | `4fbb2431d85d73015b44a3ffef260455123ed6593ba6b7b1fc7129ab77fa6753` |
| 源骨架动作版 | `95ef2d22c9ef061817073e7076c2924fdd3d6e06ccaf87405fd251bed0fbe892` |
| 动作版 | `dd4995ef0fdca1867d6802aa00b7591a6f9e64c6619637a0b087d8f8afca7fd6` |
| Face SDF 待机版 | `afc420ece28c05090aeb0c859b0bf7ffdced6f4e6dfce27cb1f6b8111ceade8d` |

## 本机环境

| 项目 | 观察结果 |
| --- | --- |
| 系统 | Windows 11，64 位 |
| 处理器与内存 | i9-13900HX，31.7 GB |
| 独立显卡 | RTX 4060 Laptop，8188 MiB 显存 |
| Blender | Steam 版 5.1.2；另有 4.2.22 LTS |
| .NET | SDK 10.0.402，运行时 10.0.13 |
| 游戏目录 | `E:/Hypergryph Launcher/games/Endfield Game` |
| UnityPlayer 版本资源 | `2021.3.34f5 (0)` |
| 游戏数据目录 | 存在 `il2cpp_data` 和 `StreamingAssets/VFS` |
| 空余磁盘 | C 约 18 GB，D 约 120 GB，E 约 239 GB |

版本资源用于定位兼容候选。
它不能单独确定游戏定制序列化格式。
数据目录存在也不能证明所有游戏内容下载完整。

## REA 证据

调用：`inspect_pe_resources`。
目标：游戏安装目录中的 `Endfield.exe`。
结果：PE32+，机器类型 `0x8664`，文件 827448 字节。
资源表有 12 项，静态资源树检查完整。

文件 SHA-256：
`66a0fcd99dbb51d01e610e78a6eebfc4f2a94521911be6356ab49ee2089d4c29`。

证据编号：
`ev_2e90f27a5b758069f3a0904539851ba7297a8535aac6f4d6e91f93008684ac7f`。

这项证据固定本机样本身份。
其覆盖范围仅为静态 PE 资源树。
Bundle 解析、游戏执行和动画求解均未观察。
后续 REA 任务按实施计划中的具体问题开展。

完整证据保存在 [REA 记录](endfield-validation/rea-evidence.json)。
资产统计保存在 [GLB 清单](endfield-validation/asset-inventory.json)。

## 上游依据

| 来源 | 本轮会话核对的版本 |
| --- | --- |
| [RuriRipperImporter](https://github.com/ShiyumeMeguri/RuriRipperImporter) | `fb77f982c97c6e3d72a998782c76c18cae39fabf` |
| [Ruri.RipperHook](https://github.com/FractalTools/Ruri.RipperHook) | `ed62f85af4e5da83dabd514b29ee93ef7d34e86b` |

Importer 文档以 Blender 5.1+ 为验证环境。
当前桥接采用 `pythonnet` 与 .NET 10。
Bin Dir 需要 DLL、依赖和 CLI 运行配置。
实际执行应固定匹配版本并记录全部文件哈希。

公开构建使用 `PureRelease=true`。
构建流程检查私有 `Ruri.GameHook` 源码缺席。
专用类型树也由非 PureRelease 条件纳入。
公开构建能否读取当前游戏仍属待验证问题。

依据见[构建流程](https://github.com/FractalTools/Ruri.RipperHook/blob/ed62f85af4e5da83dabd514b29ee93ef7d34e86b/.github/workflows/build-hooks.yml)。
项目条件见[项目文件](https://github.com/FractalTools/Ruri.RipperHook/blob/ed62f85af4e5da83dabd514b29ee93ef7d34e86b/Source/Ruri.RipperHook/Ruri.RipperHook.csproj)。
Shader 的宿主差异见[不支持项说明](https://github.com/ShiyumeMeguri/RuriRipperImporter/blob/fb77f982c97c6e3d72a998782c76c18cae39fabf/Game/Endfield/shader/Blender/_BLENDER_UNSUPPORTED.md)。

## 引擎事实与待项

加载器当前支持单 skin、四权重和两个位置形态目标。
动画通道支持 TRS，插值支持 STEP 与 LINEAR。
表情权重动画、形变法线和多目标均需扩展。
静态 GLB 加载会烘焙节点变换并合并几何。

现有材质支持 Face SDF、头发响应及透明叠层。
该能力的来源与边界见[角色渲染调查](2026-10-09-character-rendering-diagnosis.md)。
中性场景导入需要保留层级、实例及资源引用。
详细任务见[资源验证实施计划](../plans/2026-10-16-endfield-resource-validation.md)。
