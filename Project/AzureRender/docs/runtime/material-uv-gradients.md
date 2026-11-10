# 材质 UV 梯度助手

公共助手计算二维纹理取样的梯度足迹。
`MaterialUvGradientPolicy` 提供设备策略及纵向镜像粗梯度策略。
设备策略使用所属接口的隐式梯度规则。
镜像策略通过片元四像素组读取指定的细粒度梯度。

## 数值接口

`azureMirroredCoarseUvGradients(uv)` 返回一个 `mat2`。
第一列读取四像素组下行的横向细梯度。
第二列读取四像素组左列的纵向细梯度。
两列分别传给 `textureGrad` 的横向和纵向参数。

调用发生在一致控制流中、片元丢弃之前。
后续分支和覆盖判定消费已经计算的结果。
梯度保留当前坐标符号，UV 反射由调用方声明。
该接口定义取样足迹，实际层级与颜色仍由取样验证。

## 设备支持

`queryMaterialUvGradientSupport` 借用有效的物理设备句柄。
父级 Vulkan 实例版本至少为 1.1。
函数返回子组支持阶段及操作标志，不持有 GPU 资源。
设备关闭前完成能力查询及相关程序使用。

`validateMaterialUvGradientPolicy` 校验策略及能力。
镜像策略要求片元阶段与 `VK_SUBGROUP_FEATURE_QUAD_BIT`。
缺失能力和未知枚举提供明确错误。
设备策略可使用空能力记录。

片元四像素组和辅助调用规则见 [Vulkan 着色器规范](https://docs.vulkan.org/spec/latest/chapters/shaders.html)。
能力标志的含义见 [Vulkan 子组操作规范](https://docs.vulkan.org/refpages/latest/refpages/source/VkSubgroupFeatureFlagBits.html)。
调用方按策略选择匹配的程序及能力条件。

## 验证与接入

真实图形夹具使用可独立计算的双分量 UV。
两分量具有不同符号和比例，分别核对矩阵列。
偶数与奇数尺寸覆盖完整区域及边缘辅助调用。
丢弃测试在计算梯度后执行覆盖判定。

公共助手与材质、深度、阴影的生产绑定分别验收。
具体绑定持续维护各个取样域及覆盖的一致性。
当前接口属于公共渲染助手，不改变默认材质取样。
