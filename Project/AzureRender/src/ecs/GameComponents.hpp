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
AZURE_TYPE("azure.character", 3)
struct Character {
    AZURE_FIELD("Speed", 0, 100)
    float speed = 4.0F;
    AZURE_FIELD("Sprint multiplier", 1, 4)
    float sprintMultiplier = 2.5F;
    AZURE_FIELD("Jump speed", 0, 100)
    float jumpSpeed = 5.0F;
    AZURE_FIELD("Radius", 0.01, 10)
    float radius = 0.3F;
    AZURE_FIELD("Half height", 0.01, 10)
    float halfHeight = 0.6F;
    AZURE_FIELD("Acceleration", 0.01, 1000)
    float acceleration = 24;
    AZURE_FIELD("Braking", 0.01, 1000)
    float braking = 32;
    AZURE_FIELD("Turn speed degrees", 0.01, 2000)
    float turnSpeed = 540;
    AZURE_FIELD("Maximum slope degrees", 1, 85)
    float maximumSlope = 45;
    AZURE_FIELD("Step height", 0, 2)
    float stepHeight = .3F;
    AZURE_FIELD("Capsule center offset", -10, 10)
    float centerOffset = 0;
    AZURE_FIELD("Model forward yaw degrees", -180, 180)
    float forwardYaw = 0;
    AZURE_FIELD("Player controlled", 0, 1)
    bool controlled = true;
};
AZURE_TYPE("azure.third-person-camera", 1)
struct ThirdPersonCamera {
    AZURE_FIELD("Follow target", 0, 0)
    std::string target = "hero";
    AZURE_FIELD("Distance", 0.5, 30)
    float distance = 4;
    AZURE_FIELD("Minimum distance", 0.2, 30)
    float minimumDistance = 1;
    AZURE_FIELD("Maximum distance", 0.5, 50)
    float maximumDistance = 8;
    AZURE_FIELD("Shoulder offset", -3, 3)
    float shoulder = .25F;
    AZURE_FIELD("Target height", -3, 10)
    float targetHeight = 1;
    AZURE_FIELD("Minimum pitch degrees", -85, 85)
    float minimumPitch = -15;
    AZURE_FIELD("Maximum pitch degrees", -85, 85)
    float maximumPitch = 65;
    AZURE_FIELD("Sensitivity degrees per pixel", .001, 5)
    float sensitivity = .15F;
    AZURE_FIELD("Collision radius", .01, 2)
    float collisionRadius = .2F;
    AZURE_FIELD("Follow response", .01, 100)
    float response = 12;
};
AZURE_TYPE("azure.interactable", 1)
struct Interactable {
    AZURE_FIELD("Interaction range", .1, 20)
    float range = 2;
    AZURE_FIELD("Target offset", -10, 10)
    std::array<float,3> offset{0,1,0};
    AZURE_FIELD("Prompt", 0, 0)
    std::string prompt = "Interact";
    AZURE_FIELD("Enabled", 0, 1)
    bool enabled = true;
};
AZURE_TYPE("azure.collectible", 1)
struct Collectible {
    AZURE_FIELD("Category", 0, 0)
    std::string category = "artifact";
    AZURE_FIELD("Collected", 0, 1)
    bool collected = false;
};
AZURE_TYPE("azure.door", 1)
struct Door {
    AZURE_FIELD("Required count", 1, 99)
    std::uint32_t requiredCount = 3;
    AZURE_FIELD("Open", 0, 1)
    bool open = false;
};
AZURE_TYPE("azure.checkpoint", 1)
struct Checkpoint {
    AZURE_FIELD("Activated", 0, 1)
    bool activated = false;
    AZURE_FIELD("Respawn position", -10000, 10000)
    std::array<float,3> position{};
};
AZURE_TYPE("azure.task-state", 1)
struct TaskState {
    AZURE_FIELD("Started", 0, 1)
    bool started = false;
    AZURE_FIELD("Collected count", 0, 99)
    std::uint32_t collected = 0;
    AZURE_FIELD("Door opened", 0, 1)
    bool doorOpened = false;
    AZURE_FIELD("Completed", 0, 1)
    bool completed = false;
};
AZURE_TYPE("azure.script", 1)
struct Script {
    AZURE_FIELD("Asset", 0, 0)
    std::string asset;
    AZURE_FIELD("Enabled", 0, 1)
    bool enabled = true;
};
AZURE_TYPE("azure.animator", 2)
struct Animator {
    AZURE_FIELD("Graph asset", 0, 0)
    std::string asset;
    AZURE_FIELD("State", 0, 0)
    std::string state = "idle";
    AZURE_FIELD("Enabled", 0, 1)
    bool enabled = true;
    AZURE_FIELD("Locomotion driven", 0, 1)
    bool locomotion = false;
    AZURE_FIELD("Crossfade seconds", 0, 10)
    float crossfade = .18F;
    AZURE_FIELD("Walk reference speed", 0.01, 100)
    float referenceSpeed = 2;
    AZURE_FIELD("Initial clip time", 0, 600)
    float startTime = 0;
    AZURE_FIELD("Morph weight zero", 0, 1)
    float morph0 = 0;
    AZURE_FIELD("Morph weight one", 0, 1)
    float morph1 = 0;
    float playbackRate = 1;
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
