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

## 节点值快照

`RuntimeLifecycle::snapshotScene` 返回独立的场景文档。
节点变换、可见性及对象阴影取自当前世界。
失效节点通过稳定压缩处理，存活节点保持文档顺序。
失效父节点引用清空，对应灯光按节点有效性筛选。

节点名称、材质槽、已解析材质和预制来源随文档保留。
运行时持有源文档，调用方持有返回值。
修改返回节点及材质表保持源文档和已有快照的独立性。
节点数据在文档副本上更新和压缩。

CPU 分配与回归范围见[节点快照验证](../acceptance/nr/runtime-snapshot-copy.md)。
