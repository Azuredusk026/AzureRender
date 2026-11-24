# 生成几何与刚体动作

> 文档类型：教程
> 前置：已构建编辑宿主与几何编译器

## 参数化模型

1. 打开项目，确认内容目录可写。
2. 阅读 `assets_public/procedural/modular-solid.json`。
3. 选择建筑或道具的参数文件。
4. 通过共享编辑操作生成 glTF。

生成操作的参数示例如下。
`source` 填写完整源文件文本。

```json
{
  "generator": "azure.procedural",
  "output": "assets:/building.gltf",
  "parameters": {
    "source": "{\"schemaVersion\":1,\"root\":{\"op\":\"cube\",\"size\":[2,3,4]}}",
    "seed": 7,
    "budget": {"schemaVersion": 1, "timeoutMs": 60000}
  },
  "license": "项目内容的实际许可来源"
}
```

操作名为 `asset.generate`。
自动通道先查询 `document.version`。
请求携带该基础版本和唯一请求标识。
成功结果返回资产身份。

1. 用该身份调用 `preview.asset` 检查模型。
2. 用 `node.place` 放置节点。
3. 调整变换，保存项目并重新打开。
4. 用撤销验证节点与资源附加事务。

内容浏览器支持双击与拖放模型。
失败诊断可在日志中查询。
修正源后再次生成即可复用同一输出位置。
来源文件记录生成版本与输出哈希。

## 文本部件与动作

源示例位于 `assets_public/procedural/rig/articulated-tool.json`。
该源声明父子部件与两个动作。
`anim_sweep` 循环，`anim_extend` 单次播放。

1. 用 `azure.rig-text` 生成 `articulated.gltf`。
2. 用同源 `azure.rig-animation-graph` 生成动作图。
3. 放置模型，为节点添加 `azure.animator`。
4. 将 `asset` 指向动作图，设置 `state`。

可设置不同 `startTime` 验证多实例播放。
播放预览后停止，编辑文档应恢复。
独立 Player 消费已生成的标准资产。
完整字段与预算见[程序化内容契约](../runtime/procedural-content.md)。

## 自动验收入口

在工程目录执行以下命令。
输出保留两个工作流的日志与图像。

```powershell
python tools/test_procedural_host.py --executable build/ninja-msvc-debug/AzureRender.exe --output build/evolution/g10/tutorial-check
ctest --test-dir build/ninja-msvc-debug -R '^AzureAssets.(ProceduralGeometry|RigText|ProceduralProcess)$' --output-on-failure
```

脚本验证固定响应提案、失败缓存与保存重开。
生成与直接导入的 GPU 图像逐字节一致。
预制体重开可能调整运行节点数组顺序。
持久化验收对照保存文件的全部字节。
