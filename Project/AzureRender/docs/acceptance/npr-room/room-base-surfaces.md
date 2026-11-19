# 房间不透明材质底色与主纹理

> 状态为 P5 局部验收。固定镜头房间对照与启动回放通过。

房间静态网格和合批网格按源渲染器槽位绑定 30 份 lilToon 不透明材质。共设置 170 个槽位，涉及 25 张原始 sRGB 主纹理。桌面、墙面、椅子、地板、合批房间网格和道具使用源底色及主纹理。

导入器通过资产 UUID 或挂载路径定位 GLB，从实际材质名称核对槽位。合批网格的每个渲染器沿自身槽位编号绑定。材质保留源剔除状态，使用 `Hidden/ltspass_opaque/FORWARD` 的不透明覆盖规则。主纹理字节以源图像目录的 SHA-256 校验。

底色由引擎材质文档消费。阴影色阶、高光、自发光分层、透明材质和统一参考图配准按 P5 完整矩阵继续验收。

## 回放

任务 1733 固定相机和时步，采集一对 1280×720 画面。两个项目的关卡数据除新增槽位绑定外逐项一致。像素差异为 550,144 个，整幅平均线性亮度为 0.250549，基础对照为 0.163792。两侧退出时分别释放全部图像和缓冲区。

任务 1734 和 1740 各通过 850 帧房间启动、取消加载和重入。23 个输入动作、七次音源启动及七个角色部件同步均符合预期。

## 底色计算

引擎按通道查表处理大型 RGBA8 纹理，保留原有 sRGB 转换、线性乘法和量化规则。恒等颜色保持源字节。小纹理直接计算。材质文档测试核对两种颜色空间的全部 256 个通道输入码，覆盖 RGB、透明度及恒等颜色。

任务 1738 和 1741 通过材质文档、覆盖通道和无光照层测试。任务 1739 在相同工程、镜头和时步下取得完全一致的 1280×720 图像。材质准备耗时为 5.682 秒，计算对照为 75.406 秒。两个样本的逻辑材质像素量均为 2,282,257,896 字节。该数据用于核对场景准备阶段。

| 证据 | SHA-256 |
| --- | --- |
| 来源及目标槽位清单 | `59ed542cde0b60eaef5031a8fc09009a1050469cc57d99aa199305d1385a1fa8` |
| 同镜头成对回放 | `bbacfd43148b6766b806af68ae3076f978d6f188be280304c6c5a2593923303a` |
| 房间启动回归 | `1515fab0c1d13899abb85c55c99fcc9e897fca89b33a47046dc4ca5771f8c965` |
| 材质画面 | `4f2d22001af2473e931013dad2bbf30f15cdbad0c610e73e2749e98a06340284` |
| 底色计算对照 | `a40fdf086f0bc90ffd94c903a71c100f78e3ba5f21f27457f94be1e97c422ee4` |
| 底色计算房间回归 | `77c2d42818678b0a403af6a1d1052d2383ac83d4b7a5bad372f7b069fea62f2d` |

本机工程位于 `assets_private/npr-room-dev/room-base-surfaces`，画面位于 `reference-preview/room-base-surfaces.png`。
底色计算回放的 Player 与构建摘要位于该工程的 `validation/tint`。

## 镜片透明材质

房间材质装配核验 `mt_Lens.mat` 的原始身份。
原材质使用透明混合，基色 Alpha 为零。
本机文档保留深度写入及小于或等于的比较规则。
镜片沿原渲染器的稳定槽位绑定。

完整表面文档使用版本三的 `azure.profile`。
混合和深度规则由既有材质契约消费。
装配支持重复调用，并校验原文件与侧车的身份。
镜片反射的逐像素等价继续按完整材质矩阵验收。

任务 2140 在多云配准姿态中验证透明度。
共改变 7,024 个像素，均位于镜片区域。
面部区域以外的像素逐一相同。
Player 通过同步校验并释放全部图像与缓冲。

五场景的 2,000 帧回放覆盖取消、重试和二次进房。
原音乐启动两次，七个角色部件同步播放。
任务 2143 另按原音效、语音和动作目录回放三次计时事件。
2,190 帧中的提示点、原声和动作均通过对应校验。

来源收据和画面对照位于本机私有目录
`windows-atmosphere-reference/original-room-dynamics/lens/`。

## 运行时主贴图与底色

配准入口支持 `--captured-materials` 的图像目录根路径。
工具按源帧的 `_MainTex` 与 `_Color` 定位主贴图和底色。
可见刚性渲染器通过独立 `materialSlots` 变体绑定。
源材质及共享该材质的其他实例各自保有绑定。

杯子使用原版运行时的 `t_cup_kumares` 图案。
图片按冻结目录核对摘要，材质沿用既有 PBR 契约。
源图像与源材质保持只读，派生资源采用相对挂载。
派生资源侧车声明现存源文件的读取方式。

任务 2188 在两种天气中运行完整房间。
原机位、耳机、五盏灯及同帧天空云雾共同参与。
各组两帧重复图像摘要一致，Vulkan 校验错误为零。
退出后图像和缓冲全部释放。

| 天气 | 核对材质槽 | 配准材质槽 | 新增主贴图 | 变化像素 |
| --- | ---: | ---: | ---: | ---: |
| 夜晚 | 23 | 12 | 1 | 69,514 |
| 多云 | 24 | 13 | 1 | 69,730 |

163 项房间检查及 Lua 功能夹具通过。
正常配准入口与目录移动检查通过。
夹具覆盖共享实例、未知贴图、摘要损坏及发布失败。
素材索引按现存资产读取侧车，兼容临时关卡收尾后的目录。

本机目录为 `original-room-dynamics/captured-runtime-materials/`。
报告摘要为 `ff3a37f1a4087de2bb7e6bd9a13919bfd6c8460e51156a4e293f25990b77f654`。
当前范围为已匹配的不透明刚性渲染器主贴图及底色。
动态实例、变形道具及完整材质外观继续按 P4、P5 核验。

## 复验

```powershell
python -B -m tools.npr_room.room_surfaces --room $cloudyRoom --output $surfaceRoom --source $unitySource --catalog $materialCatalog --images $imageCatalog
python -B tools/npr_room/test_room_surface_player.py --player $player --base $cloudyRoom --textured $surfaceRoom --resource-root $resources --output $pair
python -B tools/npr_room/test_room_startup_player.py --player $player --project $project --resource-root $resources --output $replay --expect-heroine --expect-music
ctest --test-dir $buildDir -R '^(AzureAssets\.MaterialDocument|AzureRender\.MaterialCoverage|AzureRender\.UnlitTextureMaterial)$' --output-on-failure
```
