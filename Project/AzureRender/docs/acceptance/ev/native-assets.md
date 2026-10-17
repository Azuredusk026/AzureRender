# 原生资产获取验收

日期：2026-10-17。
状态：容器读取通过，角色与动画转换验证中。

## 来源与范围

游戏版本为本机 Endfield 1.0.14。
引擎版本为 Unity 2021.3.34f5。
源游戏目录始终只读。
原生资产与派生数据保留在本机任务目录。

采用 [Endfield Extractor](https://github.com/An1X3R/Endfield-map-extractor) v3.1。
源码提交为 `6226108267942ec940cc65b6850ea8c49198c20f`。
发行包 SHA-256 为：
`18b1c94cce3e9797af93080b1a516dba1dc3c3d7d4092d23aca1086f2f581dbe`。

发行包哈希与上游 `SHA256SUMS.txt` 一致。
上游原创源码使用 MIT，第三方组件各有许可。
依赖许可见发行包 `third_party/THIRD-PARTY-NOTICES.md`。
本仓库适配通过外部模块接口调用读取器。

证据根目录为：
`build/runs/20261017-000956-endfield-native-3caf73/`。
该目录保留原始失败日志与成功输出。

## 目录与容器

本机 VFS 有 24 个 BLC 和 385 个 CHK。
目录解码后的协议号为 3。
分组名前有四字节修订字段。
CRC 前有十四字节尾部，字段另行保留。

本机适配位于 `tools/endfield_native/compat.py`。
适配先验证 CRC、协议及头尾结构。
随后将分组主体交给上游解析器。
目录数量、文件范围和磁盘容器身份分别核对。

| 检查 | 结果 | 证据 |
| --- | --- | --- |
| BLC CRC | 24/24 通过 | `vfs-groups-legacy.json` |
| CHK 名称与长度 | 24 组全部匹配 | `vfs-groups-legacy.json` |
| 虚拟文件记录 | 274,284 条 | `vfs-groups-legacy.json` |
| 真实路径数据库 | 338,840 条 | `resources.sqlite` |
| Bundle 与 manifest 对应 | 203,075 条，双向差集为零 | `resources.sqlite` |
| 首批真实包读取 | 20 包，失败 0 | `scan-first20.jsonl` |
| 全量包扫描 | 203,075 包，失败 0 | `providers.jsonl`、`scanner.log` |
| 莱万汀路径匹配 | 1,445 个包 | `providers.jsonl` |
| 跨包索引 | 203,077 个序列化文件身份 | `cabs.jsonl` |

扫描验证包解压、序列化目录及容器路径读取。
各对象类型的语义转换需要独立验收。
标准公共包对照仍待补充。
本轮成功依据来自实际游戏包读取。

四份引导元数据均通过虚拟文件 MD5 校验。
三份最小 InitBundle 样本也通过该校验。
元数据位于 `metadata/`，样本位于 `sample/`。

## 角色与动画

角色根对象为 `chr_0016_laevat_postmodel`。
模型与根对象依赖闭包包含 78 个包。
总计 142,561,975 字节，外部未解析引用为零。
每包提取时验证索引中的内容 MD5。

角色源清单位于 `laevat-input.json`。
静态读取结果位于 `laevat-static/`。
已读出 493 个变换节点、48 个源网格与 Avatar。
转换器报告 2 个形变对象，表情语义待验证。

静态完整性检查报告材质覆盖 21/23。
纹理覆盖为 48/49，状态为 `incomplete_material_coverage`。
原生几何与材质源证据已保存。
完整角色转换验收保持开放。

三个原生动作候选已提取，依赖闭包完整。
候选为编队待机 01、编队待机 02和待机转放松。
动作选择等待用户回复。
演示表情保持中性。

编队待机 01 的原生 ACL 缓冲已解码。
变换部分包含 390 条轨道、151 帧和 60 Hz 采样。
根运动部分为 21 条，浮点部分为 17 条。
源 Avatar 已解析并保存。

上游验证器要求根运动轨道包含于浮点前缀。
该条件在本机动作上失败，完整导出状态为失败。
原始缓冲和全部采样保留于 `animation-candidates/`。
下一步验证轨道语义、绑定、坐标与引擎播放。

## 原生候选数值与播放验证

`tools/endfield_native/animation.py` 验证通用轨道候选。
支持范围为直接变换轨道和恒定单位根数据。
动态根运动、动态标量和肌肉曲线需独立求解。
原始失败记录与采样数据完整保留。

编队待机 01、02 均通过候选数值检查。
各有 390 条轨道，全部唯一对应源骨架路径。
两个动作均为 151 帧、60 Hz、2.5 秒。
分别校验 605 和 594 个源常量绑定。

待机 01 最大常量误差为 `1.4e-7`。
待机 02 最大常量误差为 `1.5e-7`。
根数据为三组恒定单位变换。
17 条标量数据均恒定，控制器语义另行保留。

`tools/endfield_native/build_preview.py` 创建技术预览。
`tools/endfield_native/export_preview.py` 输出独立 GLB。
两者通过 `--run-root` 和 `--clip` 指定输入。
Blender 使用 `--background --factory-startup` 隔离运行。

预览使用实际原生网格、蒙皮、贴图与采样姿态。
蒙皮骨架包含 313 个骨骼，未使用轨道另行登记。
待机 01 的源世界矩阵最大误差为 `3.31e-6`。
材质采用基础贴图近似，衣摆物理尚未还原。

待机 01 GLB 包含 313 个关节与 939 条动画通道。
独立项目 `engine-native-probe/` 完成五帧播放采样。
匹配构建的 Player 运行退出码为 0。
Vulkan 校验无错误，卸载后缓冲和图像计数为零。

报告为 `engine-native-report-rebuilt.json`。
截图位于 `engine-native-capture-rebuilt/`。
该结果证明技术接入，完整画面验收保持开放。
用户选择编队待机 01 与帝江号小范围切片。

首次运行使用的 Player 与共享着色器版本不匹配。
对应日志保留于 `engine-native.stderr.log`。
匹配 MSVC 14.44 环境重建后，描述符错误消失。
本轮未修改引擎源代码。

## 原生表情与场景目录

已提取 `data_facemorph_avatar_laevat.asset`。
资产绑定的 Avatar 与角色 Animator 完全一致。
表情源包含 245 个控制项和 95 个骨骼基准姿态。
247 条托管引用已完整解析，剩余字节为零。

证据位于 `expression-source/`。
控制器包含骨骼形变和着色参数映射。
这些数据的引擎驱动仍需实现与验证。
当前演示保持中性表情。

已解码关卡描述、中文文本与情绪映射表。
场景候选包括 O.M.V.帝江号、枢纽区和纯白空间。
帝江号中心切片已完成几何与材质依赖提取。
源文件指纹集中保存于 `native-evidence-manifest.json`。

## 帝江号实例、依赖与联合采样

`tools/endfield_native/scene.py` 读取原始场景布局。
本机 InitChunkData 为未压缩 FlatBuffers。
上游压缩入口在首块失败，原始失败保留。
适配验证表头后，复用上游实体与绘制参数解析。

主场景 `base01_lv001` 含 45 块、1,942 个唯一实例。
13 条重复记录的名称与矩阵一致。
读取范围覆盖全部层级后缀。
`base01_lv002` 另有 4 块、83 个唯一实例。

技术切片按源实例原点选择。
范围为 X、Z 的 `[-20,20)` 和 Y 的 `[-3,10)`。
完整网格保留，边界处允许超出选择范围。
该范围包含 87 个实例与 392 组绘制引用。

网格与材质路径由原生哈希解析，全部成功。
148 个资源路径各自对应唯一提供包。
93 个直接包扩展为 156 个依赖包。
总计 251,822,057 字节，未解析外部引用为零。

SceneProbe 输出 129 个网格、52 个材质和 124 张纹理。
输出包含依赖资源，数量不等于切片可见对象数。
预览按源 LOD 区间选择距离平方为零的绘制项。
145 个绘制对象使用原生矩阵、子网格与材质引用。

Blender 预览保存于 `dijiang-central-preview-v2/`。
`scene.blend`、`scene.glb` 与 `audit.json` 保留转换结果。
材质为上游静态近似，游戏光照和特效尚未复原。
GLB 导出存在纹理采样器合并警告。

`engine-dijiang-probe/` 加载原生角色与场景。
Player 完成五帧采样，退出码为零。
待机 01 正常推进，中性形变权重为零。
卸载后缓冲与图像计数均为零。

首次截图记录了角色穿透、脸部白块与场景色差。
该次角色 GLB 的 12 个材质被导出为混合透明。
后续不透明表面验证见下一节。
完整视觉验收仍开放。

### 角色不透明表面验证

`tools/endfield_native/material.py` 按源参数恢复不透明表面。
仅接受明确的表面类型、混合因子与裁剪状态。
真实输入有 9 个材质满足此契约。
另外 4 个材质保留原值，并登记为待验证。

`character-opaque-report.json` 保存源身份与文件指纹。
几何、动画与纹理二进制保持原始转换结果。
联合复测截图位于 `engine-dijiang-opaque-capture/`。
角色身体、脸部与头发的明显透明穿透得到修复。

Player 再次完成五帧采样，资源正常释放。
测试角色位于场景源坐标 `(-12,1,-8)`。
衣摆物理、叠加材质与场景明暗仍需验证。
上游完整材质覆盖检查仍保留失败状态。

### 场景编辑与重放验证

`tools/endfield_native/build_scene_preview.py` 构建场景候选。
输入为实例清单、SceneProbe 输出与固定上游源码。
输出包含 GLB、Blender 工程和转换审计。
`--exchange` 额外生成可编辑场景交换包。

脚本使用 `--background --factory-startup` 启动 Blender。
参数为 `--source-root`、`--probe`、`--inventory` 和 `--output`。
输出目录要求全新，源几何与纹理按对象身份解析。
`--render` 可额外生成技术预览图。

交换包包含 87 个实例父节点、145 个绘制子节点。
这些父节点包装源 ECS 实例，不代表游戏原始父子关系。
绘制项复用 60 份网格与材质组合资源。
源世界矩阵通过轴变换保留。

87 个实例的交换矩阵最大误差为 `2.98e-7`。
其中 11 个实例使用非均匀缩放。
本切片无镜像实例，镜像契约沿用公共回归。
逐顶点包围盒的最大越界量为 `2.60e-6` 米。

`engine-dijiang-editable/` 使用正式 `scene.import` 导入。
编辑器保存了 232 个节点与 60 个资源引用。
重开后移动实例 0.25 米，撤销恢复原内容哈希。
再次保存后的 JSON 与首次保存完全一致。

证据为 `dijiang-editor-roundtrip.json`。
操作请求和结果位于 `dijiang-editor-reopen/evidence.json`。
编辑器退出正常，卸载后缓冲与图像计数均为零。
源映射保留在关卡的 `sceneImports` 中。

首次装载 60 份资源约需 85 秒。
自动化先等待资源装载，再发送保存请求。
早期等待超时日志保留，未用于宣称保存成功。
装载耗时与纹理重复驻留属于后续性能待项。

重放输出位于 `dijiang-replay-verified/`。
145 个绘制项与首次预览的几何、材质身份一致。
构建脚本验证源矩阵与实际顶点包围盒。
源文件身份汇总于 `dijiang-evidence-manifest.json`。

联合工程入口为 `engine-dijiang-editable/joint.azureproject`。
它包含 61 个资源引用和 234 个关卡节点。
场景实例可单独编辑，角色播放编队待机 01。
表情保持中性，展示底座与背景已关闭。

联合 Player 完成五帧采样，退出码为零。
运行报告无呈现错误，Vulkan 日志无校验错误。
卸载后缓冲与图像计数均为零。
截图位于 `engine-dijiang-editable-capture/`。

主要证据位于当前原生任务目录：

- `dijiang-central-inventory.json`：实例、源块指纹与绘制引用。
- `all-providers.jsonl`：203,075 包全量扫描，失败为零。
- `dijiang-central-closure.json`：资源提供包与依赖闭包。
- `dijiang-central-probe/scene_manifest.json`：实际导出对象。
- `engine-dijiang-report.json`：联合项目运行状态。
- `engine-dijiang-capture/`：五帧实际引擎截图。

## 环境与测试

Python 使用 3.11，运行库使用本机 .NET 9。
隔离依赖为 pycryptodome 4.0.0、brotli 1.2.0、lz4 4.4.5。
REA 原生分析后端当前未配置。
本轮格式适配依据公开源码与本机字节校验。

适配测试命令：
`python tools/test_endfield_native.py`。
四个用例覆盖有效布局、损坏、截断和未知协议。
全部通过，运行入口帮助命令通过。

动画验证命令为 `python tools/test_endfield_native_animation.py`。
五个用例覆盖单位根、路径冲突和动态通道拒绝。
非单位旋转也有独立拒绝检查，全部通过。
两份真实待机候选完成数值与 Blender 输出验证。

原生场景与材质工具各有 4 个和 3 个回归用例。
灯光适配有 4 个用例，全部原生工具合计 20 个通过。
真实场景另有数值、Blender 重放和编辑器往返验证。

场景材质与灯光展示见[专项记录](dijiang-lighting-video.md)。
该记录包含通道烘焙、源灯光参数与照明近似范围。

`tools/endfield_native/run_upstream.py` 提供隔离入口。
指定上游源码目录、依赖目录和目标脚本。
入口仅修改当前 Python 进程中的解析函数。
原始上游源码与游戏安装目录保持可追溯。
