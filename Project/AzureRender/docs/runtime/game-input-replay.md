# 游戏输入回放与关卡等待

> 文档类型：运行时接口说明
> 状态：Implemented，受影响回归已验证

## 输入文件

Player 使用 `--game-actions <json>` 读取输入文件。
文件使用 `schemaVersion: 1` 与 `actions` 数组。
每个动作指定非负整数 `frame`。
数组按帧升序排列，同帧保持声明顺序。

回放通过正常输入动作驱动游戏。
按键、焦点和相机事件共享运行时输入规则。
关卡动作调用当前 `LevelSession`。
具体路线和关卡引用由输入文件提供。

## 关卡等待

`wait-level` 等待目标关卡完成提交。
`reference` 是项目挂载中的关卡引用。
`afterRevision` 要求提交版本大于该值。
其默认值为零，可用于首次进入目标关卡。

`timeoutFrames` 是必填的整数帧预算。
允许范围为一至一百万。
目标满足前，后续回放动作保持排队。
目标满足后，后续动作按原帧间隔执行。

以下片段演示请求后的等待与移动输入。
引用需对应项目内的实际关卡。

```json
{
  "schemaVersion": 1,
  "actions": [
    {"frame": 10, "action": "load-level", "reference": "assets:/next.azurelevel"},
    {"frame": 11, "action": "wait-level", "reference": "assets:/next.azurelevel", "timeoutFrames": 900},
    {"frame": 12, "action": "key", "key": 87, "down": true},
    {"frame": 42, "action": "key", "key": 87, "down": false}
  ]
}
```

重开同一关卡时提供当前 `afterRevision`。
这可区分既有内容与新提交的运行世界。
等待动作负责回放队列的时序。
关卡读取、上传与提交仍由关卡会话负责。

## 结果与失败

运行报告记录 `replayedActions`。
`replayDelayedFrames` 记录累计等待造成的延迟。
验收同时核对动作数量、任务结果和关卡版本。
烟雾运行的总帧数需覆盖路线与等待预算。

目标未提交且达到预算时返回等待超时。
缺少关卡会话时返回明确错误。
失败的等待动作保持未消费状态。
输入脚本的非法版本与参数在解析时拒绝。

CPU 回归使用 `AzureEngine.SystemComposition`。
完整流程使用 `AzureGame.GameSequence`。
后者通过普通交互进入探索的后继平台。
结果核验两个关卡通关和平台八枚必经金币。
