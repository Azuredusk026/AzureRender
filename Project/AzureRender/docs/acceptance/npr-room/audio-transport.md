# 音源播放控制

> 状态为 P3 局部验收。原版音乐的流式播放、暂停、调音量和切曲通过独立 Windows Player 回放。

Lua 和托管脚本使用同一音源控制接口。暂停保留采样位置，恢复沿该位置继续。调整 `azure.audio-source` 的音量字段只更新增益。单独暂停的音源在全局恢复后保持暂停。显式播放从音源开头开始。

短音效使用预解码采样，长音频使用流式解码。两种音源均使用原子采样位置供主线程读取。离线混音用例验证短音效暂停时保持静音，恢复时有输出，调零音量时播放次数和混音能量保持稳定，并验证循环回绕与单次播放的结束位置。

## 独立 Player 回放

共享队列任务 1696 使用 Recovery 的 Aqua Room 和 ChilDream。Player 启动两次流式音源，第一首暂停在约 0.06 秒，暂停期间将音量从 50% 调为 25%。恢复后采样位置前进至约 0.09 秒，再停止并替换为第二首。退出释放所有缓冲区与图像。

任务 1697 使用同一 Player 完成 850 帧启动与房间回归，消费 23 个事件，触发五次原版提示音，角色动作序列为 `[0, 1, 0]`。

| 证据 | SHA-256 |
| --- | --- |
| 流式回放报告 | `7b55eefd91acac2f8bbe1a30a8658e34b26910ef2966050010318dd675d1dbd6` |
| 房间回归报告 | `94a257468c798f2fbc827f3105ba40e8c95ed483275333c534eb32ac37fc3ebc` |
| Release Player | `1a873a9d31a5dbd23107cd8158499b9031c5a306f56761b39dcba8db40a71933` |

## 复验

```powershell
& $presentationTests $publicGameplayProject
python -B tools/npr_room/test_audio_transport_player.py --player $player --source $unitySource --resource-root $resources --output $transportReplay
python -B tools/npr_room/test_room_startup_player.py --player $player --project $progressionProject --resource-root $resources --output $roomReplay --expect-heroine
```
