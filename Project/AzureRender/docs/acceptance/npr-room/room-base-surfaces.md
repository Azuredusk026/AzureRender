# 房间不透明材质底色与主纹理

> 状态为 P5 局部验收。固定镜头房间对照与启动回放通过。

房间静态网格和合批网格按源渲染器槽位绑定 30 份 lilToon 不透明材质。共设置 170 个槽位，涉及 25 张原始 sRGB 主纹理。桌面、墙面、椅子、地板、合批房间网格和道具使用源底色及主纹理。

导入器通过资产 UUID 或挂载路径定位 GLB，从实际材质名称核对槽位。合批网格的每个渲染器沿自身槽位编号绑定。材质保留源剔除状态，使用 `Hidden/ltspass_opaque/FORWARD` 的不透明覆盖规则。主纹理字节以源图像目录的 SHA-256 校验。

底色由引擎材质文档消费。阴影色阶、高光、自发光分层、透明材质和统一参考图配准按 P5 完整矩阵继续验收。

## 回放

任务 1733 固定相机和时步，采集一对 1280×720 画面。两个项目的关卡数据除新增槽位绑定外逐项一致。像素差异为 550,144 个，整幅平均线性亮度为 0.250549，基础对照为 0.163792。两侧退出时分别释放全部图像和缓冲区。

任务 1734 通过 850 帧房间启动、取消加载和重入。23 个输入动作、七次音源启动及七个角色部件同步均符合预期。原生固定镜头采集记录材质准备耗时 75.4 秒、逻辑材质像素量 2.28 GB，装载成本继续核验。

| 证据 | SHA-256 |
| --- | --- |
| 来源及目标槽位清单 | `59ed542cde0b60eaef5031a8fc09009a1050469cc57d99aa199305d1385a1fa8` |
| 同镜头成对回放 | `bbacfd43148b6766b806af68ae3076f978d6f188be280304c6c5a2593923303a` |
| 房间启动回归 | `1515fab0c1d13899abb85c55c99fcc9e897fca89b33a47046dc4ca5771f8c965` |
| 材质画面 | `4f2d22001af2473e931013dad2bbf30f15cdbad0c610e73e2749e98a06340284` |

本机工程位于 `assets_private/npr-room-dev/room-base-surfaces`，画面位于 `reference-preview/room-base-surfaces.png`。

## 复验

```powershell
python -B -m tools.npr_room.room_surfaces --room $cloudyRoom --output $surfaceRoom --source $unitySource --catalog $materialCatalog --images $imageCatalog
python -B tools/npr_room/test_room_surface_player.py --player $player --base $cloudyRoom --textured $surfaceRoom --resource-root $resources --output $pair
python -B tools/npr_room/test_room_startup_player.py --player $player --project $project --resource-root $resources --output $replay --expect-heroine --expect-music
```
