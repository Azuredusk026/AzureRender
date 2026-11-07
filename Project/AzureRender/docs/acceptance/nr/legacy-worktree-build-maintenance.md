# 历史工作树构建维护

## 保留范围

`build/q1-ramp-source` 是独立 Git 工作树。
其提交、Git 指针及三十三个源文件改动完整保留。
当前联合场景的一千七百二十九项输入均未引用该目录。

本批处理该工作树内两个生成目录：

```text
build/q1-ramp-source/Project/AzureRender/build/ninja-msvc-debug
build/q1-ramp-source/Project/AzureRender/build/ninja-msvc-release
```

两个目录均无保留标记、Git 跟踪文件或重解析点。
执行前核对了进程占用、主构建摘要及用户差异。
清理通过既有 `artifact_cleanup.remove_artifact` 执行。

## 归档与空间

归档路径相对于引擎目录：

```text
build/archives/
```

| 生成目录 | 文件数 | 逻辑字节数 | 恢复归档 |
| --- | ---: | ---: | --- |
| Debug | 1295 | 5897894794 | `q1-debug-generated.tar.zst` |
| Release | 1432 | 1494824968 | `q1-release-generated.tar.zst` |

归档采用逐文件摘要清单和流式解包校验。
删除前再次核对源文件大小、修改时间及 SHA-256。
归档与清单摘要登记于[产物清单](../../development/artifact-manifest.json)。
恢复时按归档中的引擎相对路径展开。

D 卷实际可用空间增加 7397617664 字节，约 6.89 GiB。
删除后可用空间为 109836390400 字节。
普通任务继续执行卷容量百分之十的空间保留规则。

## 验证结果

两配置均完成运行时及材质文档目标的必要构建。
两项 `AzureAssets.MaterialDocument` 检查全部通过。
一千六百一十五项生产输入及七百七十三项主产物保持一致。
历史工作树的三十三项源文件和用户差异摘要保持一致。

本批结果只覆盖生成目录维护和对应构建完整性。
整体画质及正式帧预算继续按原验收标准推进。
[结果收据](evidence/2026-11-07/legacy-worktree-build-maintenance/results.json)
记录空间、归档和必要检查。
[验证归档](evidence/2026-11-07/legacy-worktree-build-maintenance/validation.zip)
保存脚本、日志和逐项核对收据。
