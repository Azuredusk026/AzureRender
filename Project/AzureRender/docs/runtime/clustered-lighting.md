# 聚簇光照与阴影运行时说明

## 光源数据

场景光源关联场景节点，节点世界变换决定发射位置。
颜色、强度、影响半径和启用状态随场景序列化。
渲染器按稳定标识排序光源，最多上传 128 个。

每帧将局部光源位置变换到视图空间。
聚簇索引覆盖影响体积触及的屏幕瓦片及深度层。
光源缓冲保留世界坐标、形状、颜色及角色响应。
光栅灯记录为 144 字节。

`ClusterGridDesc` 的投影倍率保留实际矩阵符号。
视空间包围盒先投影各端点，再排序屏幕区间。
透视投影使用近面截断后的深度端点。
正交投影直接变换包围区间。

已有屏幕坐标输入使用投影倍率绝对值计算半径。
矩阵纵轴与片元屏幕坐标遵守同一映射。
镜像投影对横轴和纵轴采用相同规则。
方向灯覆盖全部聚簇。

网格尺寸为 `16 × 9 × 24`。聚簇头缓冲记录索引起点与数量，索引缓冲按聚簇顺序展平。

片元通过屏幕坐标和相机前向深度查询聚簇。
命中光源按形状计算衰减、漫反射和高光。
固定与索引描述符路径绑定同一组三个存储缓冲。

## 级联阴影

方向光阴影分为四个级联，按相机视深度选择。
分割混合对数和均匀距离，权重为 `0.65`。
每帧为各实例上传四个光源视投影矩阵。
阴影通道将实例绘制到 `2 × 2` 深度图集。

每个级联包含十二次遮挡搜索及十六次 PCF 过滤。
采样限制在当前图集分区，按最大半影半径保留边界。
渲染图声明阴影图的读写状态。
角色主通道读取级联结果。

## 验证

`assets_public/scenes/clustered_lights_empty.azscene` 提供无灯场景。
`assets_public/scenes/clustered_lights_16.azscene` 提供十六个点光。
两者使用相同角色，由 `AzureRender.ClusteredLightingGpu` 比较。
`AzureRender.NullRhiPass` 检查级联视口与阴影录制。

`AzureRender.ClusteredLightProjectionGpu` 检查上下偏置灯。
CPU 检查覆盖两轴镜像、正交、透视及屏幕坐标输入。

以下命令采集四种灯数的 GPU 时间及聚簇引用数。
程序窗口以最小化方式运行。

```powershell
python tools/run_clustered_light_performance.py --executable build/msvc-release/AzureRender.exe --output-dir build/perf/R2/light-scale
```

源码入口：`src/render/ClusteredLightGrid.hpp`、`src/render/LightBuffer.hpp`、`src/scenes/CharacterSceneRenderer.cpp`。契约测试：`tests/ClusteredLightGridTests.cpp`、`tests/NullRhiPassTests.cpp`。
