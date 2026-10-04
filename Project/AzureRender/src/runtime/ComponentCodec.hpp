#pragma once
#include "ecs/Components.hpp"
#include "ecs/World.hpp"
#include "reflection/Registry.hpp"
#include "runtime/GameComponents.hpp"
namespace azurerender {
template<class Callback>
void visitComponentType(const std::string& type, Callback&& callback) {
    if (type == "azure.transform") callback(ecs::TransformComponent{});
    else if (type == "azure.renderable") callback(ecs::RenderableComponent{});
    else if (type == "azure.rigid-body") callback(game::RigidBody{});
    else if (type == "azure.character") callback(game::Character{});
    else if (type == "azure.third-person-camera") callback(game::ThirdPersonCamera{});
    else if (type == "azure.script") callback(game::Script{});
    else if (type == "azure.animator") callback(game::Animator{});
    else if (type == "azure.audio-source") callback(game::AudioSource{});
    else if (type == "azure.game-ui") callback(game::GameUiDocument{});
    else throw std::invalid_argument("Unsupported ECS component: " + type);
}
inline void validateComponents(const nlohmann::json& components) {
    if (!components.is_object()) throw std::invalid_argument("Components must be a type-keyed object");
    const auto registry = reflection::makeRuntimeRegistry();
    for (const auto& item : components.items()) visitComponentType(item.key(), [&](auto value) { registry.decode(item.key(), &value, item.value()); });
}
inline void installComponents(ecs::World& world, ecs::Entity entity, const nlohmann::json& components) {
    const auto registry = reflection::makeRuntimeRegistry();
    for (const auto& item : components.items()) visitComponentType(item.key(), [&](auto value) {
        registry.decode(item.key(), &value, item.value()); world.addComponent(entity, std::move(value));
    });
}
} // namespace azurerender
