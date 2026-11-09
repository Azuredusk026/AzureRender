# 并行资源调度验收

日期：2026-11-09。
状态：共享入口及输入保护检查通过。

## 交付范围

重负载任务由操作系统文件锁保持互斥。
受管运行器负责期限、日志及输出预算。
外部消费者使用主维护索引登记活动引用。
历史目录保持其原有生命周期。

性能、画质与论文对话均已登记实际标识。
画质流四项输入已登记租约。
工作区与所有权见 `D:/Projects/FYP-coordination/assignments.json`。
操作入口见[共享资源](../../development/shared-resources.md)。

## 验证结果

`test_shared_resources.py` 六项检查通过。
覆盖进程互斥、异常释放及持有者核对。
目录和模型清理检查覆盖外部历史输入。
`test_artifact*.py` 的 89 项回归通过，一项跳过。

跳过项为当前权限下的符号链接创建检查。
维护测试夹具使用项目内独立测试目录。
资产清理夹具具有真实 Git 仓库。
本单元未运行渲染或改变正式性能结论。

验证命令使用项目 Python 执行：

```powershell
python -B -m unittest discover -s tools -p test_shared_resources.py
python -B -m unittest discover -s tools -p 'test_artifact*.py'
```

运行记录位于 `build/runs/20261109-220746-shared-resource-validation-9ae6eb`。
所属受管收据保存命令、结果及临时目录。
