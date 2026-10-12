#pragma once
#include "foundation/Environment.hpp"
#include <imgui.h>
#include <nlohmann/json.hpp>
#include <fstream>
#include <map>
namespace azurerender::ui {
// Explicit local input replay, using the same event format as the editor.
// Only ImGui input is injected; project operations remain owned by controls.
class ProjectCenterPlayback {
public:
    void beginFrame(std::uint64_t frame,const nlohmann::json& widgets) {
        const auto path=environmentPath("AZURERENDER_PROJECT_CENTER_UI_ACTIONS");if(!path)return;
        auto& io=ImGui::GetIO();io.AddFocusEvent(true);io.AddMousePosEvent(mouse_.x,mouse_.y);
        if(release_){io.AddMouseButtonEvent(0,false);release_=false;}
        if(click_){io.AddMouseButtonEvent(0,true);release_=true;click_=false;}
        if(!std::filesystem::is_regular_file(*path))return;
        if(std::filesystem::file_size(*path)>65536)throw std::runtime_error("Project center input budget exceeded");
        nlohmann::json events;{std::ifstream input(*path);if(!input||input.peek()==std::char_traits<char>::eof())return;input>>events;}
        if(!events.is_array()||events.size()>512)throw std::runtime_error("Invalid project center input array");
        while(cursor_<events.size()&&events.at(cursor_).at("frame").get<std::uint64_t>()<=frame) {
            const auto& event=events.at(cursor_++);const auto kind=event.at("action").get<std::string>();
            if(kind=="click") {
                const auto rect=widgets.at(event.at("target").get<std::string>()).get<std::array<float,4>>();
                mouse_={rect[0]+rect[2]/2,rect[1]+rect[3]/2};io.AddMousePosEvent(mouse_.x,mouse_.y);click_=true;
            }else if(kind=="text")io.AddInputCharactersUTF8(event.at("text").get_ref<const std::string&>().c_str());
            else if(kind=="key") {
                const std::map<std::string,ImGuiKey> keys={{"A",ImGuiKey_A},{"Enter",ImGuiKey_Enter},{"Escape",ImGuiKey_Escape}};
                io.AddKeyEvent(ImGuiMod_Ctrl,event.value("ctrl",false));io.AddKeyEvent(keys.at(event.at("key").get<std::string>()),event.value("down",true));
            }else throw std::runtime_error("Unknown project center input event");
        }
    }
private:
    std::size_t cursor_=0;bool release_=false,click_=false;ImVec2 mouse_{-10000,-10000};
};
}
