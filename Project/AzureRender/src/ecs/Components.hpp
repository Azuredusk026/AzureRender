#pragma once

#include "Entity.hpp"
#include "reflection/Annotations.hpp"

#include <array>
#include <cstdint>
#include <string>
#include <vector>

namespace azurerender::ecs {

// Per-entity world-space transform. Mirrors the editor gizmo TRS so an
// entity can be positioned/rotated/scaled through ECS queries.
AZURE_TYPE("azure.transform", 1)
struct TransformComponent {
    AZURE_FIELD("Translation", -100000, 100000)
    AZURE_FIELD_EDITOR("m", 3, true)
    std::array<float, 3> translation{0.0F, 0.0F, 0.0F};
    AZURE_FIELD("Rotation", -100000, 100000)
    AZURE_FIELD_EDITOR("deg", 2, true)
    std::array<float, 3> rotation{0.0F, 0.0F, 0.0F};  // degrees
    AZURE_FIELD("Scale", -100000, 100000)
    AZURE_FIELD_EDITOR("", 3, true)
    std::array<float, 3> scale{1.0F, 1.0F, 1.0F};
};

// Marks an entity as drawable. primitiveIndex refers to
// LoadedAsset::primitives. Used by the ECS-driven render path.
AZURE_TYPE("azure.renderable", 1)
struct RenderableComponent {
    AZURE_FIELD("Primitive", 0, 4294967295)
    std::uint32_t primitiveIndex = 0;
    AZURE_FIELD("Visible", 0, 1)
    bool visible = true;
};

// Human-readable name for the Outliner/ECS bridge.
struct NameComponent {
    // Fixed-size buffer keeps NameComponent trivially copyable.
    std::array<char, 64> name{};
};

// Point light attached to a scene node entity. Position comes from that
// entity's TransformComponent; color, intensity and radius are light data.
struct LightEmitterComponent {
    std::string id;
    std::array<float, 3> color{1.0F, 1.0F, 1.0F};
    float intensity = 1.0F;
    float radius = 5.0F;
    bool enabled = true;
};

// A node may own several independently identified point-light emitters.
struct LightComponent {
    std::vector<LightEmitterComponent> emitters;
};

}  // namespace azurerender::ecs
