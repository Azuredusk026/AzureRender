#pragma once
#include "runtime/Project.hpp"
#include "runtime/SystemRegistry.hpp"
namespace azurerender::application {
SystemRegistry systems();
nlohmann::json explorationConfiguration();
nlohmann::json configuration(const Project& project);
}
