# AzureRender 使用手册

> 适用于 `0.1.0-rc1`。命令均从 `Project/AzureRender` 执行。

## 1. 运行方式

发布包用户直接运行 `bin/AzureRender.exe`。源码工作区完成安装后，使用下面两个自包含入口：

```powershell
.\build\install-debug\bin\AzureRender.exe
.\build\install-release\bin\AzureRender.exe
```

无参数时启动公共 `character` 场景和 `assets_public/test_model.gltf`，不依赖私有资产。

## 2. 常用场景

公共角色：

```powershell
.\build\ninja-release\AzureRender.exe `
  --scene-type character `
  --asset .\assets_public\test_model.gltf
```

终末地式角色展示预设：

```powershell
.\build\ninja-release\AzureRender.exe `
  --asset .\assets_public\test_model.gltf `
  --qa-camera full-body-front `
  --qa-light stylized-key
```

黑洞：

```powershell
.\build\ninja-release\AzureRender.exe `
  --scene-type blackhole `
  --blackhole-quality cinematic `
  --blackhole-camera front
```

黑洞质量可选 `performance`、`balanced`、`cinematic`；相机可选 `front`、`orbit-left`、`high`、`close`。

## 3. 角色控制

| 按键 | 行为 |
|---|---|
| `1`-`4` | 全身方向机位 |
| `5` | 脸部近景 |
| `6` | Portfolio 环绕镜头与 Endfield 预设 |
| `Left`/`Right` | 微调模型旋转 |
| `Space` | 暂停/继续自动旋转 |
| `R` | 切换自动旋转 |
| `F1` | Azure Gallery |
| `F2` | Endfield Industrial v1 |
| `F3` | Neutral Material Check |
| `F4` | 暂停/继续动画 |
| `F5`/`F6` | 调整 diffuse band |
| `F7`/`F8` | 调整 style mask |
| `F9` | 切换风格化光照 |
| `F10` | 切换内部描边 |
| `F11` | 重启动画 |
| `F12` | 保存单张 PNG |
| `H` | 显示/隐藏技术 HUD |

## 4. 编辑器场景

创建 `.azscene`：

```powershell
.\build\ninja-release\AzureRender.exe `
  --asset .\assets_public\test_model.gltf `
  --create-scene .\scenes\public_character.azscene
```

打开编辑器：

```powershell
.\build\ninja-release\AzureRender.exe `
  --editor .\scenes\public_character.azscene
```

`.azscene` 保存 renderer 类型、资源引用、节点和渲染设置。加载场景后，只有显式按键或 QA 命令才会应用整套展示预设，因此场景内自定义 grade 不会被启动流程覆盖。

## 5. 确定性截图

捕获目录必须不存在或为空：

```powershell
.\build\ninja-release\AzureRender.exe `
  --scene-type blackhole `
  --width 1280 --height 720 `
  --capture-dir .\captures\blackhole\blackhole_beauty_temporal_v1_1280x720_20260818 `
  --capture-frames 36 --capture-fps 60
```

本地捕获目录统一使用：

```text
captures/<scene>/<scene>_<view-or-purpose>_<look-or-technique>_v<NN>_<width>x<height>_<YYYYMMDD>/
```

`captures/` 可随时删除且不进入 Git。只有通过人工检查和哈希确认的单张代表图才能复制到 `portfolio/images/<scene>/`；正式文件名不带日期和任务编号。证据参数写入 `portfolio/evidence/<scene>/`。

## 6. GPU Timing

```powershell
.\build\ninja-release\AzureRender.exe `
  --scene-type blackhole `
  --gpu-timing `
  --gpu-timing-output .\captures\blackhole\blackhole_timing_rtx4060_1280x720.json `
  --smoke-frames 300
```

Timing 只表示被 timestamp 包围的 GPU pass，不等同于包含 CPU、present、readback 和编码的完整帧耗时。

## 7. QA 入口

常用机位：`full-body-front`、`face-front`、`face-three-quarter`、`back-detail`、`lighting-sweep`。

常用灯光：`neutral-material`、`stylized-key`、`specular-rim`、`rear-emissive`。

示例：

```powershell
.\build\ninja-debug\AzureRender.exe `
  --asset .\assets_public\test_model.gltf `
  --qa-camera face-three-quarter `
  --qa-light stylized-key `
  --smoke-frames 120
```

## 8. 诊断与排错

检查资源：

```powershell
.\build\ninja-release\AzureRender.exe --check-resources
```

常见诊断参数：

| 参数 | 用途 |
|---|---|
| `--diagnostic-view beauty|normal|outline|shadow` | 切换输出视图 |
| `--hud` | 开启技术 HUD |
| `--resource-root <path>` | 指定安装资源根 |
| `--smoke-frames <N>` | 固定帧数后退出 |
| `--version` | 输出版本 |
| `--help` | 输出完整命令行参考 |

`--help` 返回退出码 0；非法参数打印 usage 并返回退出码 2。

若 Windows 报告缺少 `libgcc_s_seh-1.dll`、`libstdc++-6.dll`、`libwinpthread-1.dll` 或 `glfw3.dll`，说明运行的是旧包或只复制了 EXE。请运行完整安装树的 `bin/AzureRender.exe`，不要单独移动 EXE。

## 9. 资产边界

- `assets_public/` 可以用于 CI、发布和公开截图。
- `assets_private/` 不得进入安装包、压缩包或 `portfolio/`。
- `portfolio/` 只包含 manifest 列出的公共证据。
- 第三方许可入口是 `THIRD_PARTY_NOTICES.md`。

构建环境和贡献流程见 [开发指南](DEVELOPMENT_GUIDE_CN.md)，发布门禁见 [发布与验收](RELEASE_AND_ACCEPTANCE_CN.md)。
