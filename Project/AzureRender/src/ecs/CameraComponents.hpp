#pragma once
#include "reflection/Annotations.hpp"
#include <cstdint>
namespace azurerender::game {
AZURE_TYPE("azure.camera", 1)
struct GameCamera {
    AZURE_FIELD("Enabled", 0, 1)
    bool enabled = true;
    AZURE_FIELD("Priority", 0, 1000)
    std::uint32_t priority = 0;
    AZURE_FIELD("Orthographic", 0, 1)
    bool orthographic = false;
    AZURE_FIELD("Vertical field of view", 1, 179)
    float fieldOfView = 60.F;
    AZURE_FIELD("Orthographic height", 0.001, 100000)
    float orthographicHeight = 10.F;
    AZURE_FIELD("Near plane", 0.001, 10000)
    float nearPlane = .1F;
    AZURE_FIELD("Far plane", 0.002, 100000)
    float farPlane = 1000.F;
};
}
