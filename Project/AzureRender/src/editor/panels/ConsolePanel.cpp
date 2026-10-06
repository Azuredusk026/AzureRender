#include "editor/ImGuiEditorLayer.hpp"
#include "editor/ui/Widgets.hpp"
#include "editor/ui/UiScopes.hpp"
#include "reflection/Registry.hpp"
#include "runtime/ComponentRegistry.hpp"
#include "diagnostics/RuntimeDiagnostics.hpp"
#ifdef AZURERENDER_HAS_IMGUI
#include <imgui.h>
#include <algorithm>
#include <cstring>
#include <fstream>
#include <map>
#include <set>
namespace azurerender {
#ifndef IMGUI_HAS_DOCK
namespace {
void setFallbackPanelRect(float x,float y,float width,float height) {
    const auto display=ImGui::GetIO().DisplaySize;
    ImGui::SetNextWindowPos({display.x*x,display.y*y},ImGuiCond_Always);
    ImGui::SetNextWindowSize({display.x*width,display.y*height},ImGuiCond_Always);
}
}
#endif
void ImGuiEditorLayer::drawConsolePanel(PanelContext& panelContext) {
    const auto& view=panelContext.view();
#ifndef IMGUI_HAS_DOCK
    setFallbackPanelRect(0.50F, 0.72F, 0.50F, 0.28F);
#endif
    if(!ImGui::Begin("Console###console",workspace_.open("console"))){ImGui::End();return;}
    if(auto* scripts=session_->scripts())for(const auto& error:scripts->errors())ImGui::TextWrapped("Script: %s",error.c_str());
    if(auto* levels=session_->levels())if(!levels->lastError().empty())ImGui::TextWrapped("Level: %s",levels->lastError().c_str());
    if(auto* presentation=session_->presentation())for(const auto& error:presentation->errors())ImGui::TextWrapped("Presentation: %s",error.c_str());
    consoleFilter_.Draw("Search logs",240*dpi_);observeWidget("console.search");ImGui::SameLine();
    ImGui::SetNextItemWidth(100*dpi_);ImGui::Combo("Level",&consoleLevel_,"All\0Warnings\0Errors\0");ImGui::SameLine();
    const auto messages=azurerender::RuntimeDiagnostics::instance().messages();
    auto accepts=[&](const std::string& text){
        if(!consoleFilter_.PassFilter(text.c_str()))return false;
        const bool error=text.find("ERROR")!=std::string::npos||text.find("error")!=std::string::npos;
        const bool warning=text.find("warn")!=std::string::npos||text.find("WARN")!=std::string::npos;
        return consoleLevel_==0||(consoleLevel_==1&&(warning||error))||(consoleLevel_==2&&error);
    };
    if(ui::button("Copy visible")) {std::string text;for(const auto& message:messages)if(accepts(message))text+=message+"\n";ImGui::SetClipboardText(text.c_str());}
    for(const auto& message:messages)if(accepts(message)) {
        const bool error=message.find("ERROR")!=std::string::npos||message.find("error")!=std::string::npos;
        if(error)ImGui::PushStyleColor(ImGuiCol_Text,{1,.55F,.45F,1});
        ImGui::TextUnformatted(message.c_str());if(error)ImGui::PopStyleColor();
        if(ImGui::IsItemClicked())for(std::size_t i=0;i<view.scene().nodes.size();++i)
            if(message.find(view.scene().nodes[i].id)!=std::string::npos){session_->edit("node.select",{{"index",i}});break;}
    }
    ImGui::End();
}


}
#endif
