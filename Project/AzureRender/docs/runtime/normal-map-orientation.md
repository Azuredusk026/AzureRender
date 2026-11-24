# 法线图方向转换

工具入口为 `tools/normal_map_orientation.py`。
输入为已经编码的 RGB 或 RGBA 法线图。
源方向与目标方向均须显式声明。
转换属于内容处理，渲染器沿用既有法线输入。

## 坐标约定

切线空间法线的绿色通道表示 Y 分量。
方向字符串为 `positive` 或 `negative`。
两种声明相同时，输出保留全部像素。
方向相反时，绿色字节变为二百五十五减原值。

红色、蓝色和透明度逐字节保留。
转换保持尺寸、颜色模式和原始文件。
源图像数据与目标切线坐标须共同确定方向。
图像纵向翻转与法线分量方向分别核对。

## 使用

```powershell
python tools/normal_map_orientation.py --input normal.png `
  --output normal-negative-y.png `
  --source-y positive --target-y negative
```

输出须为新 PNG 文件。
同路径、已有输出及其他输出格式进入拒绝路径。
已有来源旁车也进入拒绝路径。

灰度、整数及浮点图像需要先确定编码语义。
工具接收明确的八位 RGB 或 RGBA 像素。

旁车收据格式为 `NormalYOrientation/1`。
它记录源方向、目标方向、尺寸及文件摘要。
工具摘要用于关联实际转换实现。
资产身份及材质绑定由项目内容持有。

## 检查

五项工具检查覆盖旋转切线架和反向切线架。
方向往返须恢复所有像素字节。
同方向转换须保持独立图像对象及原始数据。
原生角色结果见[衣料法线核验](../acceptance/nr/native-normal-orientation.md)。
