# 影色查找表

影色查找表提供独立的材质颜色量。
输入为线性基色，输出为线性影色。
当前可通过 `shadow-color` 诊断视图检查。
完整角色漫反射组合由所属着色模型消费。

## 材质配置

glTF 材质的 `extras.azureRenderMaterial` 接受以下配置。
查找表可以与双图脸部距离场共同装载。

```json
{
  "schemaVersion": 2,
  "class": "face",
  "shadowColorLut": {
    "schemaVersion": 1,
    "texture": 3,
    "flipV": true
  }
}
```

`texture` 指向 glTF 纹理表中的整数索引。
`flipV` 默认为 `false`，表示输入图的行方向。
导入时按该字段归一化像素行。
未知字段、非法版本和缺失纹理均拒绝装载。

## 数据与采样

图像为水平排列的立方体切片。
高度为每轴采样数，范围为二至六十四。
宽度等于高度的平方。
红轴位于切片内部，蓝轴沿切片排列。
绿轴沿图像行递增。

源图保存 sRGB 编码颜色。
纹理采样先解码颜色，再执行线性插值。
查找坐标也由线性基色转换为 sRGB。
坐标限制在零到一之间。

每次查询使用相邻蓝切片的两次双线性采样。
红、绿坐标对齐首末纹素中心。
蓝轴使用切片间的线性插值。
采样固定为零层级、线性过滤和边缘夹取。

## 运行边界

资源加载器持有归一化的图像像素。
场景渲染器持有 GPU 图像及采样器。
场景卸载按既有在途帧规则释放资源。
固定描述符与索引描述符均使用独立的第九纹理槽。

固定布局共需要十五个图像采样器。
设备选择同时检查采样器、图像和总资源上限。
索引布局超出设备容量时选择固定布局。
既有材质使用一像素回退资源。

## 原生导入

入口为 `tools/endfield_native/shadow_lut.py`。
工具读取材质关键字、开关及纹理引用。
输入图像按来源清单校验 SHA256。
启用关系要求 `_SHADOW_LUT_TEX` 与源开关同时成立。

输入与输出路径由调用方明确指定。
输出保留原有几何、骨架和动画二进制。
收据记录源对象、图像方向和输出摘要。
此入口验证原生查表输入及传输。
角色全局参数和完整漫反射组合单独验收。

```powershell
$env:PYTHONPATH = 'tools'
python -m endfield_native.shadow_lut --input <角色文件> --source-root <来源目录> --output <新角色文件> --report <新收据文件>
```

## 验证入口

```powershell
ctest --test-dir build/ninja-msvc-debug -R 'ShadowColorLut' --output-on-failure
python -m unittest discover -s tools -p 'test_endfield_native_shadow_lut.py'
python tools/test_shadow_color_lut_gpu.py --executable build/ninja-msvc-debug/AzurePlayer.exe --require-validation
```

GPU 夹具使用独立的立方体八邻点插值作为期望值。
覆盖暗部、端点、颜色通道和源图行翻转。
同时检查两种描述符路径及场景资源释放。
收据逐项记录校验层的启用状态。
`--require-validation` 要求所有用例开启校验层。
