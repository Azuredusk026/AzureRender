# 聚簇光照与阴影运行时说明

## 光源数据

场景光源关联场景节点，节点世界变换决定点光源位置。光源颜色、强度、影响半径和启用状态随场景序列化。渲染器按稳定标识排序光源，最多上传 128 个。

每帧将点光源位置变换到视图空间，并投影到屏幕网格。光源缓冲保留世界坐标、半径、颜色和强度；聚簇索引覆盖投影球体触及的屏幕瓦片与对数深度层。网格尺寸为 `16 × 9 × 24`。聚簇头缓冲记录索引起点与数量，索引缓冲按聚簇顺序展平。

角色片元着色器使用片元屏幕坐标和相机前向深度查询聚簇，再对命中光源计算半径衰减、漫反射和高光。固定描述符表与 Bindless 描述符路径绑定同一组三个存储缓冲。

## 级联阴影

方向光阴影图分为四个级联，按相机视深度选择。分割使用对数与均匀距离混合，权重为 `0.65`。每帧为每个实例上传四个光源视投影矩阵，阴影 Pass 将它们绘制到 `2 × 2` 深度图集。

每个级联保留 12 次遮挡物搜索和 16 次 PCF 过滤。采样坐标限制在当前图集分区，并按最大半影半径留出边界。阴影图仍由 Render Graph 声明读写状态，角色主 Pass 读取级联结果。

## 验证

`assets_public/scenes/clustered_lights_empty.azscene` 与 `assets_public/scenes/clustered_lights_16.azscene` 使用相同角色资源。`AzureRender.ClusteredLightingGpu` 对比两张捕获图，验证十六个点光源对输出产生稳定影响。`AzureRender.NullRhiPass` 检查四个级联 viewport 与阴影 Pass 录制。

`python tools/run_clustered_light_performance.py --executable build/msvc-release/AzureRender.exe --output-dir build/perf/R2/light-scale` 采集 0、16、64 与 128 个局部光源的 GPU 时间和聚簇引用数。程序窗口以最小化方式运行。

源码入口：`src/render/ClusteredLightGrid.hpp`、`src/render/LightBuffer.hpp`、`src/scenes/CharacterSceneRenderer.cpp`。契约测试：`tests/ClusteredLightGridTests.cpp`、`tests/NullRhiPassTests.cpp`。
