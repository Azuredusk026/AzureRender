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
AZURE_TYPE("azure.animator", 1)
struct Animator {
    AZURE_FIELD("Graph asset", 0, 0)
    std::string asset;
    AZURE_FIELD("State", 0, 0)
    std::string state = "idle";
    AZURE_FIELD("Enabled", 0, 1)
    bool enabled = true;
};
AZURE_TYPE("azure.audio-source", 1)
struct AudioSource {
    AZURE_FIELD("Audio asset", 0, 0)
    std::string asset;
    AZURE_FIELD("Loop", 0, 1)
    bool loop = false;
    AZURE_FIELD("Autoplay", 0, 1)
    bool autoplay = false;
    AZURE_FIELD("Volume", 0, 1)
    float volume = 1.0F;
    AZURE_FIELD("Enabled", 0, 1)
    bool enabled = true;
};
AZURE_TYPE("azure.game-ui", 1)
struct GameUiDocument {
    AZURE_FIELD("Document asset", 0, 0)
    std::string asset;
    AZURE_FIELD("Enabled", 0, 1)
    bool enabled = true;
};
} // namespace azurerender::game
