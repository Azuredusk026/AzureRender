#include "ui/localization/Localization.hpp"
#include "ObjectCreationMenu.hpp"
#ifdef AZURERENDER_HAS_IMGUI
#include <imgui.h>
#endif
namespace azurerender::ui {
void objectCreationMenu(const nlohmann::json& presets,const std::function<void(const std::string&)>& create,const std::function<void(const std::string&)>& observe){
#ifdef AZURERENDER_HAS_IMGUI
    for(const auto& preset:presets){const auto id=preset.at("id").get<std::string>();
        if(ImGui::MenuItem(ui::tr((id=="empty"?std::string(ui::tr("创建空物体")):preset.at("label").get<std::string>()).c_str())))create(id);
        if(observe)observe("preset."+id);
    }
#else
    (void)presets;(void)create;(void)observe;
#endif
}
}
