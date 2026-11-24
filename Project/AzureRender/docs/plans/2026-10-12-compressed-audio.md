# G23 压缩音频与容器交换实施计划

状态：非延期自动门禁通过。

目标：公共操作支持 Opus、AAC/M4A 输入。
公共导出支持 Ogg Opus 与 MP3。
正式范围见[格式扩展计划](2026-10-11-asset-format-expansion.md)。

## 模块与契约

RenderCore 拥有解码、编码及块读取接口。
Runtime 拥有音源、混音与数据源生命周期。
编辑和独立交换工具使用同一格式注册。
两个项目验证公共操作与移动交付。

新增 `CompressedAudioAdapter.hpp/.cpp`。
新增按块读取与定位的公共音频读取契约。
原生解码、Opus 与平台解码各自持有状态。
流式播放按块读取，短声音使用预算内预解码。

修改 `AudioCodec.hpp/.cpp` 与 `AudioEncoder.cpp`。
修改 `AudioFormatAdapter.cpp` 注册新格式及设置。
修改 `AudioRuntime.cpp` 接入公共读取契约。
采样转换、取消及输出发布沿用资产事务。

Opus 固定为 1.6.1，opusfile 固定为 0.12。
LAME 固定为 3.100，使用现有动态库。
Windows AAC/M4A 评估系统媒体解码服务。
固定来源与组合许可随安装及项目包交付。

## 预算与边界

来源及编码输出各限制为 128 MiB。
单次预解码限制为 256 MiB。
采样率为 8000 至 192000 Hz，时长至两小时。
声道范围按解码器与输出编码器分别校验。

Opus 处理 pre-skip、尾部填充及 granule。
MP3 记录编码延迟及填充范围。
标签、循环及声道语义分别记录实际保真范围。
非法容器、截断、取消及预算耗尽受控失败。

## 验收与执行

- [x] 新建 `CompressedAudioFormatsTests.cpp`。
- [x] 执行公共格式发现的失败测试。
- [x] 核验固定依赖来源及分发许可。
- [x] 验证 Opus 输入、输出及精确有效帧数。
- [x] 验证 MP3 输出、延迟及有损误差。
- [x] 验证 AAC/M4A、Unicode 路径及损坏输入。
- [x] 验证采样转换、取消、长文件及流式定位。
- [x] 验证运行时混音、循环与音源释放。
- [x] 两个空项目完成导入、交换及重导入。
- [x] 执行两配置 CPU 及受影响 GPU 回归。
- [x] 验证双设备隔离移动包及完整许可。
- [x] 同步矩阵与证据，提交 `feat(g23)`。

测试入口为 `AzureAssets.CompressedAudioFormats`。
UI 回放经过公共操作、动态设置及任务反馈。
实体听感、正式性能及长跑集中在 P8。
