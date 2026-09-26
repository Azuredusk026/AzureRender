# 聚簇光照数据说明

`ClusteredLightGrid` 将视锥体划分为规则的三维网格。屏幕横纵坐标映射到 `x/y` 列，视深度使用对数区间映射到 `z` 层。每个聚簇保存稳定的光源索引，`gpuIndexData` 按聚簇顺序展平成上传缓冲。

光源索引来自排序后的 `LightBuffer`，因此相同输入在固定描述符和 Bindless 路径下保持一致。超出网格边界的坐标会钳制到边界聚簇，非法网格尺寸在构造时拒绝。

源码入口：`src/render/ClusteredLightGrid.hpp`、`src/render/LightBuffer.hpp`。契约测试：`tests/ClusteredLightGridTests.cpp`。
