#pragma once

#include "ecs/Components.hpp"
#include "ecs/World.hpp"
#include "scene/SceneComponents.hpp"

#include <cstddef>
#include <unordered_map>
#include <vector>

namespace azurerender::scene {

// Recomputes WorldTransformComponent for every entity that carries a
// TransformComponent. A parent's world matrix always lands before its
// children's, and only subtrees marked dirty (directly or through an
// ancestor) are recomputed. Returns the number of updated entities; a
// hierarchy cycle leaves its members untouched and makes the return value
// smaller than the transform-carrying entity count.
[[nodiscard]] inline std::size_t updateTransforms(
    azurerender::ecs::World& world) {
    auto& transforms =
        world.componentArray<azurerender::ecs::TransformComponent>();
    auto& hierarchies =
        world.componentArray<HierarchyComponent>();
    auto& worlds =
        world.componentArray<WorldTransformComponent>();

    std::unordered_map<azurerender::ecs::Entity,
        std::vector<azurerender::ecs::Entity>> children;
    for (const azurerender::ecs::Entity entity : hierarchies.entities()) {
        const HierarchyComponent& link = *hierarchies.tryGet(entity);
        if (link.parent != azurerender::ecs::kInvalidEntity) {
            children[link.parent].push_back(entity);
        }
    }

    std::size_t updated = 0;
    std::vector<azurerender::ecs::Entity> visiting;
    const auto visit = [&](const azurerender::ecs::Entity entity,
        const azurerender::internal::Matrix4& parentWorld,
        const bool ancestorDirty,
        auto&& self) -> void {
        if (std::find(visiting.begin(), visiting.end(), entity)
            != visiting.end()) {
            return;  // Cycle: leave the loop members untouched.
        }
        visiting.push_back(entity);

        const azurerender::ecs::TransformComponent* local =
            transforms.tryGet(entity);
        WorldTransformComponent* cached = worlds.tryGet(entity);
        bool propagated = ancestorDirty;
        if (local != nullptr && cached != nullptr) {
            if (cached->dirty || ancestorDirty) {
                cached->matrix = azurerender::internal::multiply(
                    parentWorld,
                    composeTrs(
                        local->translation, local->rotation, local->scale));
                cached->dirty = false;
                ++updated;
                propagated = true;
            }
        }
        const azurerender::internal::Matrix4& worldMatrix =
            cached != nullptr ? cached->matrix : parentWorld;
        const auto found = children.find(entity);
        if (found != children.end()) {
            for (const azurerender::ecs::Entity child : found->second) {
                self(child, worldMatrix, propagated, self);
            }
        }
        visiting.pop_back();
    };

    for (const azurerender::ecs::Entity entity : transforms.entities()) {
        const HierarchyComponent* link = hierarchies.tryGet(entity);
        if (link == nullptr || link->parent == azurerender::ecs::kInvalidEntity) {
            visit(entity, identityMatrix(), false, visit);
        }
    }
    return updated;
}

}  // namespace azurerender::scene
