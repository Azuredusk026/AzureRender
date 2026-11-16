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
语言覆盖按游戏界面的实际文本与回放核验。
本机目录位于 `assets_private/npr-room-dev/image-catalog`。
[清单摘要](image-catalog.json)记录相应产物身份。

## 复验

在 AzureRender 根目录执行以下命令。
目标使用受管任务内的新目录。

```powershell
python -B -m tools.npr_room.image_catalog --source $unitySource --inventory $referenceInventory --output $imageCatalog
python -B -m tools.npr_room.sprite_catalog --source $unitySource --inventory $referenceInventory --images $imageManifest --output $spriteManifest
```
