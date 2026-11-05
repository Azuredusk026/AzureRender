# 游戏呈现快照

`GameRuntime::renderScene` 返回当前插值场景的值。
调用方持有节点、资源及照明的场景文档。
相机与几何共享该次场景采样。

```cpp
auto scene = game.renderScene();
auto camera = game.renderCamera(scene);
```

`renderCamera(const SceneDocument&)` 借用输入至调用结束。
位置与父级变换来自输入场景。
相机组件选择和跟随策略来自当前运行时。
返回的相机数据由调用方持有。

宿主在同一线程推进模拟，然后生成本帧输入。
独立 Player 与编辑播放共同使用这条路径。
编辑场景视图按既有相机优先级选择实际显示视图。

无参数的 `renderCamera()` 获取自己的场景采样。
缺少已启用的场景相机时，选择可用的跟随相机。
两类相机均不可用时，返回明确的缺省状态。

CPU 与延期图像范围见[共享输入验证](../acceptance/nr/render-camera-snapshot.md)。
