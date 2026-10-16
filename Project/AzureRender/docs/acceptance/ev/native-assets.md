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

## 环境与测试

Python 使用 3.11，运行库使用本机 .NET 9。
隔离依赖为 pycryptodome 4.0.0、brotli 1.2.0、lz4 4.4.5。
REA 原生分析后端当前未配置。
本轮格式适配依据公开源码与本机字节校验。

适配测试命令：
`python tools/test_endfield_native.py`。
四个用例覆盖有效布局、损坏、截断和未知协议。
全部通过，运行入口帮助命令通过。

`tools/endfield_native/run_upstream.py` 提供隔离入口。
指定上游源码目录、依赖目录和目标脚本。
入口仅修改当前 Python 进程中的解析函数。
原始上游源码与游戏安装目录保持可追溯。
