#include "editor/ImGuiEditorLayer.hpp"
#include "editor/ui/Widgets.hpp"
#ifdef AZURERENDER_HAS_IMGUI
#include <imgui.h>
namespace azurerender {
void ImGuiEditorLayer::drawContentAssistancePanel(PanelContext& panel){
    if(!ImGui::Begin("Content Assistance###assistance",workspace_.open("assistance"))){ImGui::End();return;}
    if(!session_->modelAvailable()){
        ImGui::TextWrapped("Content assistance requires an enabled model service. Materials, textures and physics remain available through their authoring controls.");observeWidget("assistance.unavailable");ImGui::End();return;
    }
    const auto description=session_->edit("ai.describe");
    if(!description){ImGui::TextWrapped("%s",session_->lastError().c_str());ImGui::End();return;}
    const auto report=session_->proposalReport();const auto state=report.at("state").get<std::string>();
    ImGui::Text("Proposal: %s",state.c_str());
    if(ImGui::BeginCombo("Domain",proposalDomain_.c_str())){
        for(const auto& domain:description.value.at("domains")){const auto id=domain.at("id").get<std::string>();if(ImGui::Selectable(id.c_str(),id==proposalDomain_)){proposalDomain_=id;proposalTarget_="document";}observeWidget("ai.domain."+id);}ImGui::EndCombo();
    }observeWidget("ai.domain");
    ImGui::SetNextItemWidth(-1);ui::inputText("Stable target",proposalTarget_);observeWidget("ai.target");
    if(proposalDomain_=="physics"||proposalDomain_=="scene"){
        if(const auto* node=panel.view().selectedNode())if(ui::button("Use selected object"))proposalTarget_=node->id;observeWidget("ai.use-node");
    }else if(proposalDomain_=="material"||proposalDomain_=="texture"){
        ui::ButtonOptions pick;pick.disabled=selectedAssets_.size()!=1;
        if(ui::button("Use selected asset",pick))proposalTarget_=*selectedAssets_.begin();observeWidget("ai.use-asset");
    }
    ImGui::SetNextItemWidth(-1);ImGui::InputText("Instruction",proposalInstruction_.data(),proposalInstruction_.size());observeWidget("ai.instruction");
    ui::ButtonOptions generate;generate.disabled=state=="Generating"||session_->playing()||session_->building()||proposalInstruction_[0]=='\0';
    if(ui::button("Generate",generate))session_->edit("ai.generate",{{"runId","editor-ai-"+std::to_string(++proposalSequence_)},{"domain",proposalDomain_},{"target",proposalTarget_},{"instruction",proposalInstruction_.data()}});observeWidget("ai.generate");
    ImGui::SameLine();ui::ButtonOptions active;active.disabled=state!="Generating";
    if(ui::button("Cancel",active))session_->edit("ai.cancel");observeWidget("ai.cancel");
    ImGui::SameLine();ui::ButtonOptions ready;ready.disabled=state!="Ready";
    if(ui::button("Reject",ready))session_->edit("ai.reject");observeWidget("ai.reject");
    ImGui::SameLine();ready.disabled=ready.disabled||session_->playing()||session_->building();
    if(ui::button("Apply",ready))session_->edit("ai.apply");observeWidget("ai.apply");
    if(ImGui::CollapsingHeader("Proposal difference",ImGuiTreeNodeFlags_DefaultOpen))ImGui::TextWrapped("%s",report.value("diff",nlohmann::json::array()).dump(2).c_str());
    if(report.contains("diagnostics"))for(const auto& diagnostic:report.at("diagnostics"))ImGui::TextWrapped("%s",diagnostic.dump().c_str());
    ImGui::End();
}
}
#endif
