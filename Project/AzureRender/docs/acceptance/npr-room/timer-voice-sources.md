# 计时语音来源与选择

> 状态为 P3 局部验收。原版语音、分组选择及动作提示点已核对，房间自动播报和角色动作沿运行路线验收。

Recovery Windows 在番茄钟与正计时中使用 18 类计时语音。清单逐条登记 190 个剧本指令，引用 113 份经过原生解码的音频，保留七种语言、原剧情编号、表情、视线与唇动开关。

| 选择条件 | 原版阈值 | 触发概率 |
| --- | ---: | ---: |
| 长时创作开始 | 120 分钟 | 60% |
| 连续创作开始 | 距结束至多五分钟 | 90% |
| 短时创作后的休息 | 创作至多十分钟 | 20% |
| 长时创作后的休息 | 创作至少 50 分钟 | 20% |
| 长时创作完成 | 创作至少 240 分钟 | 60% |
| 短时创作完成 | 创作至多 40 分钟 | 20% |
| 番茄钟中途结束 | 计时标记为中途结束 | 50% |

Lua 选择器按原选择器的顺序判断。番茄钟与正计时分别保存随机候选，按原本地化标识去除两种模式共享的台词。测试覆盖边界时间、首轮条件、选择优先级、成组去重和随机列表重建。

## 动作提示点

清单将 16 个动作编号指向原状态和动画片段。每个等待动作的指令均有源 `PlayPomodoroVoice` 提示点。动作 67 的提示点 2 位于起始片段，提示点 1 位于循环片段，清单保留原退出时刻和状态转场。四个先说话再触发动作的指令保留其时序标记。

任务 1756 和 1757 生成相同的来源清单，SHA-256 为 `e1383d4cd4603a46472874542038dda34ed9aaac287cfa21cdb31006cadbf73f`。清单位于 `assets_private/npr-room-dev/source-lock/timer-voice-motions.json`。动作采样、播放中断和房间内提示点对账按 P3 至 P4 运行流程验收。

## 复验

```powershell
python -B -m tools.npr_room.timer_voice --source $unitySource --inventory $stories --audio-catalog $audioCatalog --audio-probe $audioProbe --animator $animator --output $receipt
python -B tools/npr_room/test_pomodoro_runtime.py --lua $luaLibrary
```
