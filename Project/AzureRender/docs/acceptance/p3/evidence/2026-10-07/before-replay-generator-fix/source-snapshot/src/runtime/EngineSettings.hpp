#pragma once
#include "foundation/SettingRegistry.hpp"
#include "render/RenderSettings.hpp"
#include <functional>
namespace azurerender {
void registerEngineSettings(SettingRegistry& registry);
RenderSettings resolveRenderSettings(const SettingRegistry& registry,RenderSettings authored);
class RenderSettingOverlay {
public:
    void apply(const SettingRegistry& registry,RenderSettings& settings);
private:
    struct Active { nlohmann::json baseline,last; };
    std::map<std::string,Active> active_;
};
}
