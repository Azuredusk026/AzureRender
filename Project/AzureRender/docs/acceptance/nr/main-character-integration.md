# 主场景角色材质接入

日期：2026-10-22。范围：NR8-V2 首批内容接入。
本页登记主关卡与可玩关卡的实际引用和专项验证。
完整原生画面与跨编辑入口一致性继续验收。

## 内容与边界

工作项目为以下目录：

```text
build/runs/20261022-051129-character-logistics-restoration-0f2e71/joint-candidate/
```

主项目启动 `candidate:/scene.azurelevel`。
可玩项目为 `playable-safe.azureproject`。
两关卡的角色引用为 `character:/hero-hair-linear-212213.glb`。
材质槽覆盖数量均为零。

该资产包含身体水平及脸部径向环境法线。
五份服饰使用独立镜面输入，头发次级颜色为线性值。
原生来源见[脸部](face-radial-indirect.md)、[服饰](cloth-specular-input.md)和[头发](projected-hair.md)证据。
环形多层高光的完整公式仍归属 C17。

候选资产的 SHA-256 为：

```text
a8a08d008eca0ec50078af5d98121cad5a4f293f5e785cb22fc3ee4a2e081619
```

接入校验覆盖网格、节点、骨架、动画及访问器。
全部非纹理缓冲视图按字节比较一致。
每份关卡仅更改一处角色资源引用。
灯光、相机、运动及其他场景配置保持同一输入。

## 运行记录

接入前后分别启动实际主项目与可玩项目。
捕获采用 1920×1080、固定时间步与场景相机。
本批使用同一 Player 二进制，未混入自发光修复构建。
运行收据记录二进制、项目、关卡、模型和图像摘要。

```text
build/runs/20261022-230758-main-material-integration-8fd37c/
```

`promotion.json` 记录运行状态及每份捕获结果。
`effective-content-inputs.json` 保存材质声明与着色器摘要。
该输入记录不替代 GPU 描述符的实际抓帧证据。
`promote.py` 保留检查和复现命令，原关卡快照同时保存。

主场景接入后，脸部、身体及服饰可见响应产生变化。
四次捕获全部通过运行及资源释放检查。
主关卡变化像素数为 75335，可玩入口为 102896。
该统计包含反射等间接影响，不限定于角色屏幕区域。
完整指纹见[结构化收据](evidence/2026-10-22/main-character-integration.json)。
原生预览与本场景的姿态和照明不同。
本批不提供原生像素等价结论。
距离描边、眼睛、透明层、GI 和反射保持所属任务验收。
