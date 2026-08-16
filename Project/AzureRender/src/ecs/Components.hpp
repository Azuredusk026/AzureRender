#pragma once

#include "Entity.hpp"

#include <array>
#include <cstdint>

namespace azurerender::ecs {

// Per-entity world-space transform. Mirrors the editor gizmo TRS so an
// entity can be positioned/rotated/scaled through ECS queries.
struct TransformComponent {
    std::array<float, 3> translation{0.0F, 0.0F, 0.0F};
    std::array<float, 3> rotation{0.0F, 0.0F, 0.0F};  // degrees
    std::array<float, 3> scale{1.0F, 1.0F, 1.0F};
};

// Marks an entity as drawable. primitiveIndex refers to
// LoadedAsset::primitives. Used by the ECS-driven render path.
struct RenderableComponent {
    std::uint32_t primitiveIndex = 0;
    bool visible = true;
};

// Human-readable name for the Outliner/ECS bridge.
struct NameComponent {
    // Fixed-size buffer keeps NameComponent trivially copyable.
    std::array<char, 64> name{};
};

}  // namespace azurerender::ecs