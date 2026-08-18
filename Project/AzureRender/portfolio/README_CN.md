# AzureRender 公开视觉证据

本目录只保存可公开、可校验的代表图与机器可读证据。临时帧序列、调试 probe、私有角色截图和视频不进入 Git。

## 目录

```text
portfolio/
  images/
    blackhole/   黑洞最终画面
    character/   公共测试资产的角色 renderer 画面
  evidence/
    blackhole/   黑洞配置、确定性与性能摘要
    character/   角色预设和视角摘要
  portfolio_manifest.json
```

## 文件命名

正式图像统一使用：

```text
<scene>_<view-or-purpose>_<look-or-technique>_v<NN>_<width>x<height>.png
```

禁止使用 `P1`、`S36`、`CQ0`、时间戳或 `final_final` 一类任务过程名称。版本号只在有意改变画面基准时递增。

## 当前证据

- `blackhole_temporal_beauty_v1_1280x720.png`：黑洞 TAA/bloom 最终 Beauty。
- `character_endfield_public_fullbody_v1_1280x720.png`：公共资产全身视角。
- `character_endfield_public_closeup_v1_1280x720.png`：公共资产近景视角。

图片来自项目公共资产，不含 `assets_private/` 内容。具体参数和 SHA-256 见 `evidence/` 与 `portfolio_manifest.json`。

## 校验

从项目根执行：

```powershell
powershell.exe -NoProfile -ExecutionPolicy Bypass `
  -File .\tools\verify_portfolio.ps1
```

架构、运行方法和未来路线分别见 `../docs/ARCHITECTURE_CN.md`、`../docs/USER_GUIDE_CN.md` 和 `../docs/DEVELOPMENT_ROADMAP_CN.md`。
