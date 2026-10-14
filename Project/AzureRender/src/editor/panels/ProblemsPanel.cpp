#include "editor/ImGuiEditorLayer.hpp"
#include "ui/Widgets.hpp"
#ifdef AZURERENDER_HAS_IMGUI
#include <imgui.h>
namespace azurerender {
void ImGuiEditorLayer::drawProblemsPanel(PanelContext&){
    if(!ImGui::Begin("Problems###problems",workspace_.open("problems"))){ImGui::End();return;}
    ImGui::SetNextItemWidth(-1);ui::inputText("Search problems",problemSearch_);observeWidget("problems.search");
    const auto report=session_->diagnostics().query({{"text",problemSearch_}});
    ImGui::Text("Capacity: %u | Dropped: %u",report.at("capacity").get<unsigned>(),report.at("dropped").get<unsigned>());
    for(const auto& problem:report.at("entries")){
        const auto id=problem.at("id").get<std::string>();ImGui::PushID(id.c_str());
        const auto label=problem.at("source").get<std::string>()+" | "+problem.at("code").get<std::string>();
        if(ImGui::Selectable(label.c_str()))session_->edit("diagnostic.reveal",{{"id",id}});observeWidget("problem."+id);
        ImGui::TextWrapped("%s",problem.at("message").get<std::string>().c_str());
        if(!problem.at("node").get<std::string>().empty())ImGui::TextDisabled("Node: %s",problem.at("node").get<std::string>().c_str());
        if(!problem.at("path").get<std::string>().empty())ImGui::TextWrapped("%s : %u",problem.at("path").get<std::string>().c_str(),problem.at("line").get<unsigned>());
        if(problem.at("occurrences").get<unsigned>()>1)ImGui::TextDisabled("Occurrences: %u",problem.at("occurrences").get<unsigned>());
        ImGui::Separator();ImGui::PopID();
    }
    ImGui::End();
}
}
#endif
