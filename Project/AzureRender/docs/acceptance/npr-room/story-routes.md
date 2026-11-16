# 剧情命令与跳转

> 状态为 P0 静态证据。房间剧情运行、触发条件与读档按 P3 验收。

Recovery `CommandType` 为剧情主表实际使用的 44 种命令提供名称。报告保留原表行号、剧情组、选项连续区间及跳转目标。

`ScenarioReader.SelectionJump` 按当前剧情组内的源表顺序查找第一个目标。报告保留其行号和同组同名行。普通选项与教学选项使用各自的连续区间，教学选项同时记录 `Arg2` 的已选标识。日期命令记录本机时间窗口、窗口内目标和窗口外目标。循环台词命令从 `Arg3` 指定的行开始收集连续普通选项。

| 来源结构 | 数量 |
| --- | ---: |
| 普通选项 | 73 |
| 直接跳转 | 10 |
| 教学选项 | 4 |
| 日期分支 | 1 |
| 循环台词选项入口 | 1 |
| 连续选项区间 | 28 |
| 组内目标引用 | 90 |

90 个目标引用均能在所属剧情组内定位，目标行各有唯一匹配。教学组 `tutorial_01` 有 12 条显式分支记录。`main_32` 含循环台词选项入口。两次独立生成报告使用相同 SHA-256 `974c7a366206a2a318c36a5413595e0df99f892426ec77af4119db6ca8eda27e`。

## 复验

```powershell
python -B -m tools.npr_room.story_routes --source $unitySource --inventory $storyInventory --output $storyRoutes
python -B -m unittest tools.npr_room.test_story_routes
```

本机报告位于 `assets_private/npr-room-dev/source-lock/story-routes.json`。
