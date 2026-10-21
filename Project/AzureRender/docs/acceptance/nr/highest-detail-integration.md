# 最高细节中庭联合接入

日期：2026-10-21。
状态：正式导入与原生运动混合短测通过。
画面等价、移动安全与发布验收继续实施。

## 资源与导入

中央区包含 86 个源实例、142 个绘制项。
上层区包含 53 个源实例、87 个绘制项。
两区提供 108 份几何资源。
场景另保留原生角色和单发射器物流资源。

资源通过编辑器生产操作 `scene.import-start` 导入。
任务状态通过 `tasks.describe` 查询。
该入口仅返回任务，适用于大型场景。
完整工作区观察仍遵守 64 KiB 文本预算。

保存后的场景包含 418 个节点、110 份资源。
两份导入基线分别包含 228 项和 140 项。
项目采用位于项目目录内的相对资源挂载。
正式 Player 的项目校验通过。

烘焙资源保留源几何、节点和材质槽身份。
源场景、转换器及输出摘要由清单记录。
中央区与上层区分别记录 45 个和 40 个源材质。
上层交互扫光采用明确标记的静态候选。

## 玻璃与照明

五份资源绑定同一个原生折射玻璃材质。
绑定依据为源容器、对象编号及网格材质槽。
五份相似名称资源绑定另一不透明材质。
身份与配置由独立核验脚本逐项检查。

联合场景保留原生局部灯、主方向光及作用体积。
间接光采用 5,733 个规则网格采样。
局部反射采用白黄色区域的原始 HDR 层级。
原生组合、有效状态和视觉误差仍需分项验收。

## 原生动作短测

Release Player 完成 180 帧、1280×720 的图像采集。
输入包含前进、跑步切换和释放，共四项操作。
角色消费原生待机、完整行走及完整跑步。
三个主导片段编号为 `0`、`7`、`8`。

权重归一最大误差为 `3.9406586438417435e-8`。
脚本与表现错误均为零。
进程正常退出，图像和缓冲释放计数均为零。
该构建按配置关闭 Vulkan 校验层。

本短测验证资源、姿态与输入混合的联合消费。
深井边缘碰撞与相机随动尚未通过验收。
实体输入、脚底接触与最终性能保留独立门禁。
阶段状态按 NR0 至 NR12 的完整条件维护。

## 证据位置

根目录为 `build/runs/20261021-172156-native-render-evidence-623c52/`。

| 文件 | 内容 |
| --- | --- |
| `high-detail-joint/import-verification.json` | 正式导入、基线与场景摘要 |
| `high-detail-joint/editor-import-current-release-tasks/` | 导入操作、日志和编辑器图像 |
| `high-detail-joint/glass-configuration-receipt.json` | 折射材质身份与资源核验 |
| `high-detail-joint/central-materials/rebake-manifest.json` | 中央区来源与烘焙配置 |
| `high-detail-joint/upper-materials/rebake-manifest.json` | 上层区来源与烘焙配置 |
| `high-detail-joint/playable-preparation.json` | 可玩变体、动作与参数摘要 |
| `high-detail-joint/playable-release/verification.json` | 180 帧运动混合及剩余限制 |
| `high-detail-joint/playable-release/runtime.json` | 每帧角色、相机及动画样本 |
| `high-detail-joint/playable-release/frames/` | 逐帧图像与采集清单 |

源几何选择见[几何细节证据](source-geometry-detail.md)。
动作来源见[原生移动动作](native-locomotion.md)。
正式完成条件见[可玩还原计划](../../plans/2026-10-17-native-playable-restoration.md)。
