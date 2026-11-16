# 正计时

> 状态为 P3 局部验收。正计时、模式切换、暂停、结束和存档重开通过独立 Player 回放。

空闲时按 M 在番茄钟和正计时之间切换，模式单独保存。
正计时从零开始，按时、分、秒显示工作时长。
T 开始、暂停或恢复，R 结束。
暂停期间保持显示值，暂停与结束使用原结束音效，开始与恢复使用原开始音效。
达到 99:59:59 后的下一秒结束本次计时。
以上行为依据 Recovery 的 `CountupTimerService`、`CountupService` 和 `TimerParentUI`。

连续工作每满 60 秒写入进度，正常退出时保存活动回合的整秒数。
重开保留模式，将上次进度收录为最近一条工作记录并进入空闲状态。
本地存档保留最近的计时记录，累计统计、跨日分配与解锁按对应验收项实现。

## 回放

共享队列任务 1635 依次运行三次独立的 Release Player。

| 运行 | 存档和音频 | 运行报告 SHA-256 |
| --- | --- | --- |
| 正常结束 | 记录 3 秒工作，播放五次原音效 | `8ce3cacc80d86136a1b5886860d356404cc8769fd15c0e348666a6ffb7835705` |
| 活动回合退出 | 保存 3 秒活动进度，播放一次开始音效 | `f75deba6052cdcbd567d9ccdf2982356afed0500dcc7be1c77ba9ad6dc3c841c` |
| 再次重开 | 收录 3 秒工作，保持空闲，音效启动次数为零 | `163baf99f402b22b5efbce17398c0c5fa386244e007d03d1c033f086fd19f55e` |

三次运行均无脚本或表现错误，退出后缓冲与图像计数均为零。
本机工程位于 `assets_private/npr-room-dev/room-countup`，移动后通过项目校验。

```powershell
python -B tools/npr_room/test_pomodoro_runtime.py --lua $luaDll
python -B tools/npr_room/test_countup_player.py --player $player --project $roomProject --output $countupReplay --resource-root $releaseBuild
```
