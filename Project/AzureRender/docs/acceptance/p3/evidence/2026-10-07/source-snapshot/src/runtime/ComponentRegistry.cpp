#include "runtime/ComponentRegistry.hpp"
#include "runtime/GeneratedComponents.hpp"
namespace azurerender {
void ComponentRegistry::add(ComponentBinding value) {
    if(sealed_)throw std::logic_error("Component registration is sealed");
    metadata_->type(value.type);
    if (!value.install || !value.encode || !value.remove || !value.contains || !value.defaults || !value.validate)
        throw std::invalid_argument("Incomplete component binding: " + value.type);
    const auto name = value.type;
    if (!bindings_.emplace(name, std::move(value)).second) throw std::invalid_argument("Duplicate component binding: " + name);
}
const ComponentBinding& ComponentRegistry::binding(const std::string& type) const {
    auto found = bindings_.find(type);
    if (found == bindings_.end()) throw std::invalid_argument("Unknown component binding: " + type);
    return found->second;
}
void ComponentRegistry::install(const std::string& type, ecs::World& world, ecs::Entity entity, const Json& value) const {
    if (!world.valid(entity)) throw std::invalid_argument("Invalid component entity");
    binding(type).install(world, entity, value);
}
Json ComponentRegistry::encode(const std::string& type, const ecs::World& world, ecs::Entity entity) const {
    if (!world.valid(entity)) throw std::invalid_argument("Invalid component entity");
    return binding(type).encode(world, entity);
}
void ComponentRegistry::remove(const std::string& type, ecs::World& world, ecs::Entity entity) const {
    if (!world.valid(entity)) throw std::invalid_argument("Invalid component entity");
    binding(type).remove(world, entity);
}
bool ComponentRegistry::contains(const std::string& type, const ecs::World& world, ecs::Entity entity) const {
    const auto& value = binding(type);
    return world.valid(entity) && value.contains(world, entity);
}
Json ComponentRegistry::defaults(const std::string& type) const { return binding(type).defaults(); }
void ComponentRegistry::validate(const std::string& type, const Json& value) const { binding(type).validate(value); }
void ComponentRegistry::validateWrite(const std::string& type, const std::string& field, const Json& value, bool toolAccess) const {
    const auto& descriptor = metadata_->type(type);
    for (const auto& property : descriptor.properties) {
        if (property.name != field) continue;
        if (property.readOnly || (toolAccess && !property.toolVisible)) throw std::invalid_argument("Property is not writable: " + field);
        if (!property.validate) throw std::invalid_argument("Property has no validator: " + field);
        property.validate(value); return;
    }
    throw std::invalid_argument("Unknown property: " + field);
}
Json ComponentRegistry::describe(const std::string& type) const {
    binding(type);
    const auto& descriptor = metadata_->type(type);
    Json properties = Json::array();
    for (const auto& property : descriptor.properties)
        properties.push_back({{"name", property.name}, {"label", property.label}, {"kind", static_cast<int>(property.kind)},
            {"minimum", property.minimum}, {"maximum", property.maximum}, {"category", property.category},
            {"tooltip", property.tooltip}, {"readOnly", property.readOnly}, {"toolVisible", property.toolVisible}});
    return {{"type", type}, {"id", descriptor.id}, {"version", descriptor.version}, {"properties", properties}};
}
ComponentRegistry makeRuntimeComponentRegistry() {
    ComponentRegistry result(reflection::makeRuntimeRegistry()); registerGeneratedComponents(result); return result;
}
ComponentRegistry& runtimeComponentRegistry() { static auto registry = makeRuntimeComponentRegistry(); return registry; }
}
