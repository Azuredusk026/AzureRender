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

Sprite 的矩形与边距进入图集对账。
语言覆盖按游戏界面的实际文本与回放核验。
本机目录位于 `assets_private/npr-room-dev/image-catalog`。
[清单摘要](image-catalog.json)记录相应产物身份。

## 复验

在 AzureRender 根目录执行以下命令。
目标使用受管任务内的新目录。

```powershell
python -B -m tools.npr_room.image_catalog --source $unitySource --inventory $referenceInventory --output $imageCatalog
```
