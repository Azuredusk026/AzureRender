# 参考裁剪坐标投影

公共内核投影已经相机相对化的三维位置。
调用方提供同一坐标系中的四列矩阵。
矩阵定义轴向及设备深度约定。
抖动使用左上角原点的归一化 UV 单位。

## 配置与布局

`ReferenceClipProjectionProfile` 的版本为一。
CPU 配置包含版本、三个保留字及 GPU 参数。
保留字均为零，矩阵与抖动均须为有限值。
未知版本及非法数值通过异常报告。

GPU 参数由四个四维列向量和一行抖动组成。
布局为八十字节，对齐为十六字节。
抖动行的前两项位于负一至一之间。
其余两项为保留值，保持为零。

## 运算顺序

计算先取位置 Y 与第二列的单精度乘积。
位置 X 与第一列通过融合乘加累积。
位置 Z 与第三列使用同样的融合乘加。
第四列以独立的单精度加法加入结果。

抖动先乘以投影后的 W。
X 通过融合乘加减去两倍横向抖动。
Y 通过融合乘加加入两倍纵向抖动。
Z 与 W 保留矩阵投影的结果。

CPU 在独立运算之间保存单精度结果。
GLSL 通过 `precise` 和具名局部值声明边界。
局部值具有显式 `NoContraction` 来源标记。
非规格化值遵循所属设备的浮点规则。
具体输入域的精度通过对应设备资格核对。

## 着色程序准备

程序创建前调用 `prepareReferenceFmaShader`。
输入为通过编译器验证的 SPIR-V 字节。
准备器将局部值标记归属到对应的乘加结果。
每个被标记值具有一个直接乘加定义。

返回值拥有准备后的字节及乘加结果数量。
重复准备保持相同字节，释放由调用方负责。
截断、非法头部和缺失来源标记提供明确异常。
准备过程完成后，再通过 RHI 创建模块。

```cpp
const auto source = readBinaryFile(path);
const auto prepared = prepareReferenceFmaShader(source);
const auto module = backend.createShaderModule(prepared.code);
```

首次编译和热重载使用同一准备步骤。
默认场景程序沿用所属创建契约。
来源标记见 [GLSL 指令扩展](https://docs.vulkan.org/glslext/latest/glslext/ext/GLSL_EXT_spirv_intrinsics.html)。
运算重组规则见 [Vulkan 浮点环境](https://docs.vulkan.org/spec/latest/appendices/spirvenv.html)。

## 调用与生命周期

CPU 调用先验证配置与位置的有限性。
中间投影或抖动溢出时提供明确异常。
GPU 调用方保证配置及输入处于有效运算范围。
正交、透视与离线来源核对共用该接口。

内核不持有 GPU 资源。
宿主拥有参数、输入位置、输出及所属命令。
参数在对应 GPU 工作结束前保持有效。
帧投影与真实深度的生产绑定分别验收。

现行场景相机继续使用所属投影接口。
本内核的来源核对见原生画质实施计划。

[实际顶点读回](../acceptance/nr/actual-vertex-clip-readback.md)
在两设备上核对完整索引输出及遮罩深度身份。
