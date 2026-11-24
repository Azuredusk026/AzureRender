#pragma once
#include "ecs/ObjectShadow.hpp"
#include "runtime/ComponentRegistry.hpp"
#include <optional>
namespace azurerender {
inline ecs::ObjectShadowComponent decodeObjectShadowComponent(const Json& envelope) {
    ecs::ObjectShadowComponent value;
    runtimeComponentRegistry().metadata().decode("azure.object-shadow",&value,envelope);
    value.validate();return value;
}
inline Json encodeObjectShadowComponent(const std::optional<ecs::ObjectShadowComponent>& value) {
    if(!value)return nullptr;
    value->validate();
    return runtimeComponentRegistry().metadata().encode("azure.object-shadow",&*value);
}
inline std::optional<ecs::ObjectShadowComponent> objectShadowFromComponents(const Json& components) {
    if(!components.contains("azure.object-shadow"))return {};
    return decodeObjectShadowComponent(components.at("azure.object-shadow"));
}
}
