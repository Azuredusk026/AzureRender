# F3 仓库与文档基础计划

> 文档类型：阶段计划
> 状态：Complete
> 更新日期：2026-10-05
> 适用范围：仓库目录、忽略规则、README 与 CI
> 需求依据：[编辑器与可玩体验优化计划](2026-10-05-quality-round.md)

## 范围与删除契约

受版本控制的交付内容为源码、配置、资产许可、维护文档和有效证据。删除对象须登记精确路径、用途、引用与恢复方式。历史实现由 Git 保留，活动证据使用稳定路径。

| 路径或类别 | 计划操作 | 判定依据 |
| --- | --- | --- |
| `.codemaker/codemap/` 五个跟踪文件 | 从版本控制移除，根规则忽略工具缓存 | 日志、缓存、锁文件为机器状态 |
| `.claude/`、`opencode.json` | 核对交付分支树和发布入口的清理结果 | 工作树与本地 `origin/main` 分别登记 |
| `docs/archive/legacy/*.md` | 列出引用后，删除失效说明并同步归档索引 | 构建、导航、脚本、活动说明与 manifest 引用检查 |
| `AfterglowRender/` | 按本机参考输入登记，README 描述按交付树编写 | 当前为忽略目录，来源与本机使用独立核验 |
| 原始 DOCX、用户素材与私有资产 | 按来源资料登记 | 项目文档文件卫生规则 |
| LegacyDescriptors、LegacySkinning 测试 | 按活动能力登记 | 固定描述符、顶点蒙皮与设备降级仍有回归入口 |
| 无引用脚本与生成目录 | 依清单清理可重建产物 | 文件用途、生产引用与重建命令 |

`.gitignore` 按构建、工具状态、私有资源、捕获和凭据分组。采用匹配实际目录的规则。`.codemaker/` 加入根目录忽略规则。候选删除规则须用 `git check-ignore` 验证输入与产物的边界。

## 文件与接口

| 操作 | 路径 | 职责 |
| --- | --- | --- |
| 修改 | 根 `.gitignore`、工程 `.gitignore` | 忽略规则与现有目录对应 |
| 修改 | 根 `README.md`、工程 `README.md`、`docs/index.md` | 项目定位、入口、结构与兼容范围 |
| 修改 | `.github/workflows/ci.yml` | `engine-dev` 推送触发与必需测试发现 |
| 修改 | `docs/archive/README_CN.md`、`mkdocs.yml`、`CMakeLists.txt` | 删除后引用、导航与安装文档 |
| 新增 | `tools/audit_repository.py` | 报告跟踪缓存、失效链接和忽略规则探针 |
| 修改 | `tools/check_docs.sh` | 执行仓库卫生与文档入口检查 |
| 新增 | `docs/acceptance/f3/<date>.md` | 精确删除清单与验证证据 |

审计命令为 `python tools/audit_repository.py --source ../.. --output build/f3/audit.json`。报告字段为 `trackedToolState`、`danglingLinks`、`ignoreProbes` 和 `status`。发现工具状态或失效入口时返回非零退出码。

## 任务一：清单与目录整理

- [x] 用 Git 跟踪清单、CMake、导航、脚本与文档建立引用表。
- [x] 保存当前卫生审计失败报告，明确五个工具状态文件。
- [x] 按清单移除跟踪缓存、失效说明和无引用可重建产物。
- [x] 整理两层忽略规则，验证源码可跟踪、生成目录被忽略。
- [x] 检查全部删除路径的引用与安装清单，登记实际结果。

审计必须区分活动回退与历史说明。固定描述符和顶点蒙皮测试仍须被发现。原始资料和本机参考目录进入来源清单。

## 任务二：README 与 CI

- [x] 按总计划定义的章节重写两个 README，首页链接到教程与演示入口。
- [x] 在兼容表中分别列出构建支持和实际验收范围。
- [x] 为 `engine-dev` 添加 CI 推送触发，校验 Release 可玩包测试发现。
- [x] 执行卫生审计、文风、严格站点构建及现有两配置回归。
- [x] 同步总计划、CHANGELOG、F3 证据与删除清单，提交 F3。

F3 的图片引用采用现有已核验公开媒体。最终编辑器与玩法展示图片由 P2 捕获并登记。README 的构建命令在当前工程验证。

## 验收

审计 `trackedToolState` 与 `danglingLinks` 均为空。忽略探针覆盖源码、公开资产、构建目录与本机缓存。CI 检查覆盖 Debug 和 Release 测试发现。GPU 回退能力及已有场景回归通过。

```powershell
git ls-files .claude .codemaker opencode.json
git check-ignore Project/AzureRender/build/probe.txt
git diff --check
python -m mkdocs build --strict --site-dir build/f3/site
```

提交前核对删除路径、Git 暂存范围和本机来源清单。阶段提交标题为 `feat(f3): 完成仓库整理与文档基础验收`。
