# Windows 窗景大气来源

> 状态为 P5 局部验收。Windows 的原始着色器程序、资源绑定和四个时段的运行参数已核对。画面对照使用对应的 Vulkan 场景验收。

`RoomScene` 中的 `MyCozyWeatherService` 绑定 `BaseCozy Weather Sphere` 和四个时段的时间模块。Windows 实例使用天空、云层和雾三个网格渲染器。三个原材质分别引用 `Stylized Sky Desktop`、`Stylized Clouds (COZY Desktop)` 和 `Stylized Fog (Desktop)`，基准天气组件引用十张原图。移动端备用天空的四个场景对象按独立引用登记。

| 时段 | 窗景编号 | 时间模块 | 原当日比例 |
| --- | ---: | ---: | ---: |
| 白天 | 11 | 22587 | 0.52083330 |
| 傍晚 | 12 | 22526 | 0.77083330 |
| 夜晚 | 13 | 25873 | 0.90921134 |
| 多云 | 14 | 25805 | 0.52083330 |

## 原程序与参照

从 Recovery 1.17.3 `ReferenceProbe` 的 `resources.assets` 读取五个原着色器对象。每个原始对象的 SHA-256 与 Recovery 归档一致。Microsoft Direct3D 反汇编器验证五组首通道的共十段着色器程序。记录包含顶点与片元程序的原条目编号、常量缓冲区偏移、纹理槽与采样器槽。两次独立提取的清单摘要一致。

四份原实机大气记录提供采样后的月亮方向、天空及云层颜色、雾、散射和滤色值。原窗景截图与运行参数使用相同的编号。首通道绑定与源资源清单用于复验 Windows 绘制参数。

| 记录 | SHA-256 |
| --- | --- |
| Direct3D 程序和绑定 | `dc42aaa993c5d494d88e7b4ac7e026b4ca41e6b4e874962a9fbdc7d42b1bca49` |
| 实机大气来源及资源绑定 | `44811daf277a26a5e6def1c1bb26ef5b9ed0f5a49514a54ced7609966badc011` |

## 复验

原 DXBC、反汇编和绑定清单写入本机私有目录。`PYTHONPATH` 指向本项目的 UnityPy 转换工具目录。

```powershell
python -B -m tools.npr_room.unity_d3d11_programs --source $resourcesAssets --manifest $originalShaderInventory --output $programFolder
python -B -m tools.npr_room.windows_atmosphere --source $unitySource --catalog $assetCatalogRoot --captures $windowMatrixIsolated --d3d-programs "$programFolder/windows-atmosphere-programs.json" --output $atmosphereReceipt
python -B -m unittest tools.npr_room.test_unity_d3d11_programs
```
