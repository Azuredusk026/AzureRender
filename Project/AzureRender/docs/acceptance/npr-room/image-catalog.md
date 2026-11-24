# 纹理与源字体目录

> 状态为 P2 局部验收。PNG 和源字体已经写入本机私有目录。

源文件来自锁定的场景及目录闭包。
本轮登记 570 张 PNG 和七个源字体。
每个 Unity GUID 对应一个稳定资源槽。
去重后共 574 个文件，合计 250,805,857 字节。

PNG 保留原始内容，并记录尺寸、模式和 sRGB 标记。
源字体保留 OpenType 或 TrueType 内容。
工具核对字符映射表、轮廓表边界和字形数量。
发布前逐份校验内容摘要。

目录闭包中的 851 个精灵登记原纹理、逻辑矩形、打包矩形、轴心和边距。
全部打包矩形通过纹理尺寸对账。
打包范围按 Unity 单精度坐标覆盖边界，原始坐标同时保存在清单中。
界面中的图集采样和透明边距按画面回放验收。
本机目录位于 `assets_private/npr-room-dev/image-catalog`。
[清单摘要](image-catalog.json)记录相应产物身份。

## 剧情字符覆盖

从 `ProjectLifetimeScope` 的 `FontSupplier` 装配读取主字体，并按序遍历回退字体。
动态图集的字形候选采用源轮廓的 Unicode 映射，由原生 FreeType 检查。 [FreeType 字符映射](https://freetype.org/freetype2/docs/reference/ft2-character_mapping.html)

2,111 行剧情本地化的七个文本列均通过静态字符对账。
12 个字体资产共有 55,362 条字符记录，字形引用和图集边界均通过。
所有图集的媒体尺寸与字体声明一致。
语言菜单、合成字形、实际排版和换行按界面回放验收。

## Alpha8 图集

两个内嵌 Alpha8 纹理转换为 PNG。
转换按行坐标写入透明度通道，并反向读取核对每个透明度字节。 [Unity Alpha8](https://docs.unity3d.com/2022.3/Documentation/ScriptReference/TextureFormat.Alpha8.html) · [Unity 像素行顺序](https://docs.unity3d.com/2022.3/Documentation/ScriptReference/Texture2D.GetPixels.html)
其中繁体中文动态字体图集来自 Recovery 生成的 1024 × 1024 纹理。
另一个字体纹理为 256 × 256。
两项纹理及清单位于本机私有目录的 `alpha-atlas`。

## 复验

在 AzureRender 根目录执行以下命令。
目标使用受管任务内的新目录。

```powershell
python -B -m tools.npr_room.image_catalog --source $unitySource --inventory $referenceInventory --output $imageCatalog
python -B -m tools.npr_room.sprite_catalog --source $unitySource --inventory $referenceInventory --images $imageManifest --output $spriteManifest
python -B -m tools.npr_room.alpha_atlas --source $unitySource --inventory $referenceInventory --texture $traditionalAtlas --texture $fontTexture --output $alphaCatalog
python -B -m tools.npr_room.font_coverage --source $unitySource --inventory $referenceInventory --story $storyManifest --images $imageManifest --freetype $freeTypeDll --output $fontCoverage
```
