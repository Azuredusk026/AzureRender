# 原材质目录

> 状态为 P2 局部验收。材质参数和网格渲染器引用已对账。

材质目录登记 258 个源文件和对应的 GUID。
记录包括着色器引用、父材质、着色器关键字、渲染队列、通道开关以及数值、颜色和纹理参数。
纹理记录保留源文件 ID、缩放和偏移。
254 个着色器指向索引中的源文件，另有四个指向 Unity 内置着色器。

十个源场景和预制体文件中含 399 个静态网格渲染器和 36 个蒙皮网格渲染器。
按序记录 447 个材质槽，其中 442 个指向目录中的材质，五个为空。
渲染器记录保留启用状态和静态合批范围。

669 个纹理参数引用已写入的位图。
另外六个非空引用由一个运行时渲染纹理、两个字体纹理和三个 Unity 内置纹理引用组成。

## 不透明材质

八个使用标准金属度流程的不透明 URP Lit 材质已转换为 `azure.pbr`。
保留源颜色、金属度和双面设置，粗糙度按一减平滑度计算。
一张基础色位图按源 GUID 绑定，写入时校验 PNG 摘要。
生产材质校验器成功读取八个文档。
其余两个 URP Lit 材质进入待转换记录，对应自定义渲染标记和超出当前不透明转换范围的颜色。

通过原变换标识及导入映射，将 `lambert1` 绑定至房间中两个蛋糕和一个烘焙杯的原网格槽。
生产编辑器保存并重开场景后保留三项绑定。
Release Player 在 1280×720 下生成两张画面，退出后缓冲与图像计数均为零。
模型和材质引用在移入本机私有目录后再次通过项目校验。
夹具位于 `assets_private/npr-room-dev/room-camera-pbr`，相关摘要见[场景资源清单](scene-assets.json)。
着色器效果与最终像素对照按渲染验收矩阵执行。

## 复验

在 AzureRender 根目录执行命令，输出写入受管任务中的新文件。

```powershell
python -B -m tools.npr_room.material_catalog --source $unitySource --inventory $referenceInventory --images $imageManifest --output $materialManifest
python -B -m tools.npr_room.pbr_materials --materials $materialManifest --images $imageManifest --output $nativeMaterials
```

两次源文件扫描的清单摘要均为 `9b82140b7a1052c473e8c495e19b7c7839fd7b49b2403a14d3c6fa7d860c352e`。
清单存放于本机私有目录的 `source-lock/material-catalog.json`。
