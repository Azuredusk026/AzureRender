# 游戏呈现快照

`GameRuntime::renderScene` 返回当前插值场景的值。
调用方持有节点、资源及照明的场景文档。
相机与几何共享该次场景采样。

```cpp
auto scene = game.renderScene();
auto description = scene.renderDescription();
auto camera = game.renderCamera(description);
```

`renderCamera(const scene::SceneDescription&)` 借用物化输入至调用结束。
宿主同时将同一份描述交给本帧几何消费者。
`renderCamera(const SceneDocument&)` 按需转换调用方文档。
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

物化输入的兼容与短时成本见[相机输入验证](../acceptance/nr/materialized-camera-input.md)。

## 专用输出复用

`RuntimeSceneSnapshotCache` 为宿主线程的专用文档提供准备状态。
输出对象的地址在缓存生存期内保持稳定。
文档发布使用 `exchange`，内容及对应状态一同交换。
外部修改或替换输出内容前调用 `clear`。
公开独立副本接口继续适用于调用方自行编辑的文档。

运行时维护不透明的源身份。
场景初始化、替换及节点新增会更新该身份。
缓存保存实体代际，当前世界决定节点、父级及灯光有效性。
组件移除时使用作者声明的变换和可见性默认值。

`RuntimeRenderSnapshotCache::prepare` 返回共享只读的渲染描述。
静态解析器保持节点和灯光顺序，并提供绑定后的资源路径。
源身份或成员布局变化时重新执行静态解析。
每帧刷新节点动态字段，以及灯光的世界位置和方向。
已发布帧由共享所有权保护，后续刷新使用可用槽位。
复用池至多保存三个槽位，额外保留帧由调用方独立持有。

完整原生结果及边界见[宿主准备验证](../acceptance/nr/host-scene-preparation.md)。
