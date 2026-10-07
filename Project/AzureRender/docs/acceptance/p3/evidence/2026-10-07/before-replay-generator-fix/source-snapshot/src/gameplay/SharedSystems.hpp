#pragma once
#include "runtime/SystemRegistry.hpp"
namespace azurerender::gameplay {
std::unique_ptr<IRuntimeSystem> characterMovementSystem(const nlohmann::json& config);
std::unique_ptr<IRuntimeSystem> locomotionSystem(const nlohmann::json& config);
std::unique_ptr<IRuntimeSystem> cameraFollowSystem(const nlohmann::json& config);
void registerSharedSystems(SystemRegistry& registry);
}
