# 反射与序列化

> 文档类型：运行时说明
> 状态：生效
> 更新日期：2026-10-03
> 适用范围：Windows 宿主工具与运行时组件
> 源码入口：`src/reflection/`、`tools/metagen/main.cpp`
> 关联测试：`ReflectionTests.cpp`、`test_metagen.py`

## 职责与使用场景

AzureMetaGen 从组件头文件生成属性注册代码，Registry 提供元数据、JSON 存档和字段编辑。Inspector 的变换控件读取生成的标签与范围。

## 数据与所有权

类型名称是存档标识，类型数字标识使用 FNV-1a 64 位计算。Registry 持有类型、字段和迁移函数，组件由调用者持有。类型名称和数字标识都须唯一。

## 生命周期与时序

CMake 在输入头文件或生成器变化时生成注册头文件。输出内容相同时保留文件时间戳，失败输入保留有效输出。运行时通过 `makeRuntimeRegistry()` 注册组件。

## 接口契约

`AZURE_TYPE` 声明稳定名称和正整数版本。`AZURE_FIELD` 声明标签与数值范围，字段随后使用受支持的类型定义。支持 float、bool、string、uint32 与三维 float 数组，每个输入文件使用一个限定命名空间。

类型存档包含 `type`、`version` 和 `data`。读取检查类型、版本、未知字段、数值范围和有限值。写入先验证候选组件，全部字段通过后提交。省略字段保留目标对象当前值。

## 线程与同步

注册与迁移配置在主线程初始化，配置完成后的只读注册表可供并行查询。组件写入由 World 所有者安排。

## 序列化与兼容

迁移按版本递增执行，缺失迁移或未来版本返回错误。版本迁移回调处理数据对象，类型名称跨文件移动保持稳定。场景的 `.azscene` 格式由 SceneDocument 管理。

## 平台行为

生成器使用 C++17 标准库，在 Windows 宿主运行。受限注解覆盖当前组件类型，解析器对无效注解提供文件、行和列诊断。语法范围与当前组件定义相匹配，工具依赖保持为标准库。Android 为 Deferred。

## 使用示例

```cpp
auto registry = azurerender::reflection::makeRuntimeRegistry();
azurerender::ecs::TransformComponent transform;
auto data = registry.encode("azure.transform", &transform);
registry.decode("azure.transform", &transform, data);
```

## 诊断与排错

生成错误先检查诊断指出的注解行。未知字段检查拼写和迁移配置，范围错误检查 Inspector 声明的边界。

## 验收与证据

运行 `ctest --test-dir build/ninja-msvc-debug -R AzureEngine.Reflection`。增量生成与错误输出由 MetaGenerator 契约测试覆盖，完整记录见 G1 验收。

## 参考来源

生成器与 Registry 为项目实现。JSON 使用 vcpkg 固定的 nlohmann/json，数据结构参考项目 ECS 组件和 Inspector 的属性需求。
