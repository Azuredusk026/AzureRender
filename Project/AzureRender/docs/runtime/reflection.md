# 反射与序列化

> 文档类型：运行时说明
> 状态：生效
> 更新日期：2026-10-06
> 适用范围：Windows 宿主工具与运行时组件
> 源码入口：`src/reflection/`、`tools/metagen/main.cpp`
> 关联测试：`ReflectionTests.cpp`、`test_metagen.py`

## 职责与使用场景

AzureMetaGen 从组件头文件生成属性注册代码，Registry 提供元数据、JSON 存档和字段编辑。Inspector 的变换控件读取生成的标签与范围。

## 数据与所有权

类型名称是存档标识，类型数字标识使用 FNV-1a 64 位计算。Registry 持有类型、字段和迁移函数，组件由调用者持有。类型名称和数字标识都须唯一。

## 生命周期与时序

CMake 在输入头文件或生成器变化时生成注册头文件。输出内容相同时保留文件时间戳，失败输入保留有效输出。运行时通过 `makeRuntimeRegistry()` 注册组件。

`ComponentRegistry` 将类型描述与 ECS 能力绑定。默认注册由生成器的组件绑定头提供。`runtimeComponentRegistry()` 持有宿主使用的注册表。编辑、脚本与加载器共用其能力。

应用在打开项目或启动宿主前完成扩展注册。宿主随后调用 `seal()` 固定类型与迁移集合。封存后的注册得到拒绝。只读查询可供后台加载使用。

## 接口契约

`AZURE_TYPE` 声明稳定名称和正整数版本。`AZURE_FIELD` 声明标签与数值范围，字段随后使用受支持的类型定义。支持 float、bool、string、uint32 与三维 float 数组，每个输入文件使用一个限定命名空间。

`AZURE_FIELD_META` 可追加分类、提示、只读和工具可见性。参数顺序为分类、提示、只读、工具可见性。两项状态采用布尔字面量。字段默认可写并向工具显示。

只读字段允许从合法存档加载。编辑器与脚本的字段写入检查只读权限。工具可见性控制属性控件与工具写入权限。完整内容校验仍由类型和领域契约负责。

组件注册使用 `registerComponent<T>(type)`。类型名、版本、字段和回调由注册表校验。`install()`、`encode()` 与 `remove()` 操作有效实体。`defaults()` 提供合法类型的初始存档。

`validateWrite()` 校验单个字段的权限与数据。`validate()` 校验完整候选及迁移。`describe()` 返回类型 ID、版本与属性规则。类型化绑定在校验失败时保持已有组件值。

已有组件的候选从当前值复制。省略属性与运行期状态保持现值。完整存档覆盖其中声明的属性。自定义绑定负责同等校验与提交规则。

新增编译期注解由 AzureMetaGen 生成反射与组件绑定。生成选项 `--components-output <文件>` 指定绑定输出。外部组件也可在应用模块中显式注册。该入口采用进程内 C++ 版本契约。

类型存档包含 `type`、`version` 和 `data`。读取检查类型、版本、未知字段、数值范围和有限值。写入先验证候选组件，全部字段通过后提交。省略字段保留目标对象当前值。

## 线程与同步

角色组件当前版本为 4，动画组件为 3。角色版本 1 至 3 迁移后具有输入配置名。动画版本 1、2 迁移后具有动画驱动配置名。迁移规则保留各版本的速度和状态语义。

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

应用可在宿主启动前登记自己的组件。下例通过公共类型化入口定义检视标记。

```cpp
#include "runtime/ComponentRegistry.hpp"

struct InspectionLabel {
    std::string text = "untitled";
};

void registerInspectionComponent() {
    using namespace azurerender;
    auto text = reflection::withMetadata(
        reflection::property<InspectionLabel>(
            "text", "Text", &InspectionLabel::text, 0, 0),
        "Inspection", "Project inspection label", false, true);
    runtimeComponentRegistry().registerComponent<InspectionLabel>(
        reflection::reflectedType<InspectionLabel>(
            "tool.inspection-label", 1, {std::move(text)}));
}
```

注册函数由应用的初始化流程调用。编辑器和 Player 宿主各自链接该注册代码。组件存档使用声明的稳定名称。实例修改仍由世界和文档的所有者执行。

## 诊断与排错

生成错误先检查诊断指出的注解行。未知字段检查拼写和迁移配置，范围错误检查 Inspector 声明的边界。

## 验收与证据

运行 `ctest --test-dir build/ninja-msvc-debug -R AzureEngine.Reflection`。增量生成与错误输出由 MetaGenerator 契约测试覆盖，完整记录见 G1 验收。

ComponentRegistry 与 MetadataGeneration 覆盖注册、权限和生成描述。RegisteredComponentWorkflow 覆盖外部类型的编辑与保存。该夹具还验证 Lua 读写和 Play/Stop 的文档隔离。

RegisteredComponentHost 使用独立注册的 Vulkan 编辑宿主。它验证属性面板绘制、字段编辑及只读拒绝。完整阶段状态见[F4 验收](../acceptance/f4/2026-10-06.md)。

## 参考来源

生成器与 Registry 为项目实现。JSON 使用 vcpkg 固定的 nlohmann/json，数据结构参考项目 ECS 组件和 Inspector 的属性需求。
