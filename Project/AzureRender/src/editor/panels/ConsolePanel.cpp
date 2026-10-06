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
    if(session_->developerServices().report){
        const auto developer=session_->developerServices().report();
        if(developer.at("shaderAvailable").get<bool>()){
            ImGui::Text("Shader reload: %s",developer.at("shaderState").get<std::string>().c_str());
            if(ui::button("Rebuild shaders"))session_->edit("developer.shader-rebuild");observeWidget("developer.shader-rebuild");
            const auto diagnostic=developer.at("shaderDiagnostic").get<std::string>();if(!diagnostic.empty())ImGui::TextWrapped("%s",diagnostic.c_str());
        }
    }
    if(session_->modelAvailable()){
        const auto report=session_->proposalReport();const auto state=report.at("state").get<std::string>();
        ImGui::Text("Content assistance: %s",state.c_str());
        ImGui::SetNextItemWidth(160*dpi_);ImGui::Combo("Domain",&proposalDomain_,"Scene assembly\0Asset parameters\0");observeWidget("ai.domain");
        ImGui::SetNextItemWidth(-1);ImGui::InputText("Instruction",proposalInstruction_.data(),proposalInstruction_.size());observeWidget("ai.instruction");
        ui::ButtonOptions generate;generate.disabled=state=="Generating"||session_->playing()||session_->building()||proposalInstruction_[0]=='\0';
        if(ui::button("Generate",generate))session_->edit("ai.generate",{{"runId","editor-ai-"+std::to_string(++proposalSequence_)},
            {"domain",proposalDomain_==0?"scene":"asset-parameters"},{"target","document"},{"instruction",proposalInstruction_.data()}});
        observeWidget("ai.generate");ImGui::SameLine();
        ui::ButtonOptions active;active.disabled=state!="Generating";
        if(ui::button("Cancel",active))session_->edit("ai.cancel");observeWidget("ai.cancel");ImGui::SameLine();
        ui::ButtonOptions ready;ready.disabled=state!="Ready";
        if(ui::button("Reject",ready))session_->edit("ai.reject");observeWidget("ai.reject");ImGui::SameLine();
        ready.disabled=ready.disabled||session_->playing()||session_->building();
        if(ui::button("Apply",ready))session_->edit("ai.apply");observeWidget("ai.apply");
        if(ImGui::CollapsingHeader("Proposal difference"))ImGui::TextWrapped("%s",report.value("diff",nlohmann::json::array()).dump(2).c_str());
        if(report.contains("diagnostics"))for(const auto& diagnostic:report.at("diagnostics"))ImGui::TextWrapped("%s",diagnostic.dump().c_str());
        ImGui::Separator();
    }
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
