#pragma once
#include "reflection/Annotations.hpp"
#include <array>
#include <cstdint>
#include <string>
namespace azurerender::game {
AZURE_TYPE("azure.rigid-body", 1)
struct RigidBody {
    AZURE_FIELD("Half extent", 0.01, 10000)
    std::array<float, 3> halfExtent{0.5F, 0.5F, 0.5F};
    AZURE_FIELD("Dynamic", 0, 1)
    bool dynamic = false;
    AZURE_FIELD("Trigger", 0, 1)
    bool trigger = false;
};
AZURE_TYPE("azure.character", 1)
struct Character {
    AZURE_FIELD("Speed", 0, 100)
    float speed = 4.0F;
    AZURE_FIELD("Jump speed", 0, 100)
    float jumpSpeed = 5.0F;
    AZURE_FIELD("Radius", 0.01, 10)
    float radius = 0.3F;
    AZURE_FIELD("Half height", 0.01, 10)
    float halfHeight = 0.6F;
};
AZURE_TYPE("azure.script", 1)
struct Script {
    AZURE_FIELD("Asset", 0, 0)
    std::string asset;
    AZURE_FIELD("Enabled", 0, 1)
    bool enabled = true;
};
} // namespace azurerender::game
