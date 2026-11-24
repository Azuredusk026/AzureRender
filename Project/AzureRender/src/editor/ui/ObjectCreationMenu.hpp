#pragma once
#include <nlohmann/json.hpp>
#include <functional>
#include <string>
namespace azurerender::ui {
void objectCreationMenu(const nlohmann::json& presets,
                       const std::function<void(const std::string&)>& create,
                       const std::function<void(const std::string&)>& observe={});
}
