#pragma once
#include "runtime/ComponentRegistry.hpp"
namespace azurerender {
inline void validateComponents(const nlohmann::json& components) {
    if (!components.is_object()) throw std::invalid_argument("Components must be a type-keyed object");
    const auto& registry = runtimeComponentRegistry();
    for (const auto& item : components.items()) registry.validate(item.key(), item.value());
}
inline void installComponents(ecs::World& world, ecs::Entity entity, const nlohmann::json& components) {
    validateComponents(components);
    const auto& registry = runtimeComponentRegistry();
    for (const auto& item : components.items()) registry.install(item.key(), world, entity, item.value());
}
} // namespace azurerender
