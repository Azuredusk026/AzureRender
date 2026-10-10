# 平台跳跃关卡

> 文档类型：运行与制作说明
> 项目：Azure Sky Steps
> 平台：Windows

## 启动与操作

独立项目位于 `assets_public/platformer/`。
探索通关后按 E 进入平台关卡。
探索完成时按 R 可重新开始探索。
平台中的 R 与重开按钮重新开始平台关卡。

```powershell
./build/ninja-msvc-release/AzurePlayer.exe --project assets_public/platformer/project.azureproject
```

WASD 移动，空格跳跃，Shift 冲刺。
鼠标旋转相机，滚轮调整相机距离。
Esc 释放光标，点击游戏区域恢复控制。
右键漂浮导航用于编辑态场景视窗。

## 关卡路线

主路包含八个平台和七次必经跳跃。
顺序为认识、练习、水平运动和组合运用。
随后学习垂直运动，完成最终挑战与奖励。
终点蓝色灯光与门楣提供远处地标。

主路提供八枚金币，两枚金币位于侧边。
金币用于指引路线，终点按到达条件判定。
金币转动并在拾取时播放短音效。
完成界面显示金币数量与重开说明。

起点是默认重生位置。
第五个平台设置中途检查点。
掉落至负五米时返回最近检查点。
掉落保留金币，重开恢复完整关卡。

## 实测与布局

实测使用生产角色系统与 Jolt 固定步进。
步长为六十分之一秒，角色胶囊高 1.8 米。
半径为 0.3 米，步行速度为每秒四米。
冲刺速度为每秒 5.6 米。

步行跳跃水平距离约 4.07 米。
跳高约 1.23 米，滞空约 1.02 秒。
停止输入后的制动距离约 0.22 米。
初次运动输入在下一个固定步生效。

间隙比例依次为 50%、55%、62% 和 65%。
随后为 67%、78% 和 60%。
前期落点宽六米，后期宽 4.8 米。
高度与间隙通过实际连续路径联合验证。

第四个平台水平移动，行程为 1.3 米。
速度为每秒 0.3 米，两端停留 1.5 秒。
第六个平台垂直移动，行程为 0.35 米。
速度为每秒 0.18 米，两端停留 1.5 秒。

具体参数见项目的 `PLATFORMER-DESIGN.json`。
移动路径由项目脚本消费设计数据。
平台碰撞与承载由公共物理模块提供。
编辑器的碰撞辅助可检查平台与角色形状。

## 生成与验证

先构建角色测量与路径验证目标。
测量文件保存速度、轨迹与制动结果。
生成器使用该文件确定平台间隙。
Blender 生成平台与金币，脚本合成五类音效。

```powershell
python tools/measure_character_jump.py --executable build/ninja-msvc-debug/AzurePlatformerPhysicsTests.exe --output build/movement.json
python tools/generate_platformer.py --output assets_public/platformer --measurements build/movement.json --blender E:/Blender/blender.exe --exploration assets_public/exploration
ctest --test-dir build/ninja-msvc-debug -R AzureGame.Platformer --output-on-failure
ctest --test-dir build/ninja-msvc-debug -R AzureGame.GameSequence --output-on-failure
```

Blender 参数使用本机安装路径。
生成来源与资源哈希保存在 `GENERATION.json`。
资源通过资产挂载进入独立项目与探索项目。
独立运行和连续切关共用同一 Player 宿主。

路径工具通过正式按键驱动游戏。
它验证必经落点、金币、承载和掉落恢复。
音频夹具验证五类播放与实际混音能量。
设备性能与实体操作范围见阶段验收记录。

## 引擎契约

刚体版本三提供 `kinematic` 字段。
动态和运动学模式互斥。
运动学目标由当前场景变换提供。
物理世界根据固定步计算实际支撑速度。

角色移动使用相对支撑面的主动速度。
公共物理模块组合主动速度与支撑速度。
跳跃和地面接触保持公共角色契约。
脚本通过 `grounded()` 与 `velocity()` 查询状态。

胜负、金币数量和重生规则属于项目脚本。
场景切换经关卡会话提交并释放资源。
公开素材、脚本与合成音效采用 CC0 协议。
参考实现与边界见[物理制作](physics-authoring.md)。
