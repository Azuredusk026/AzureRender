#include "editor/ImGuiEditorLayer.hpp"
#include "editor/ui/Widgets.hpp"
#ifdef AZURERENDER_HAS_IMGUI
#include <imgui.h>
namespace azurerender {
void ImGuiEditorLayer::drawScriptAuthoringPanel(PanelContext& panel){
    if(!ImGui::Begin("Script Authoring###scripts",workspace_.open("scripts"))){ImGui::End();return;}
    if(!panel.view().isProject()){ImGui::TextUnformatted("Open a project to create scripts.");ImGui::End();return;}
    ImGui::BeginDisabled(session_->playing()||session_->building());
    if(ImGui::BeginCombo("Backend",scriptTemplateBackend_.c_str())){
        for(const auto& backend:session_->scriptAuthoring().describe()){
            const auto id=backend.at("id").get<std::string>();
            if(ImGui::Selectable(backend.at("label").get<std::string>().c_str(),id==scriptTemplateBackend_)){scriptTemplateBackend_=id;scriptTemplatePath_="assets:/NewBehaviour"+backend.at("extension").get<std::string>();}
            observeWidget("script.backend."+id);
        }ImGui::EndCombo();
    }observeWidget("script.backend");
    ImGui::SetNextItemWidth(-1);ui::inputText("Virtual output",scriptTemplatePath_);observeWidget("script.template-path");
    if(ui::button("Create script")){const auto result=session_->edit("script.template",{{"backend",scriptTemplateBackend_},{"path",scriptTemplatePath_}});if(result)session_->edit("script.reveal",{{"asset",result.value.at("asset")}});}observeWidget("script.create");
    ImGui::EndDisabled();
    if(ui::button("Reload backend"))session_->edit("script.reload",{{"backend",scriptTemplateBackend_}});observeWidget("script.reload");
    if(const auto* selected=panel.view().selectedNode()){
        for(const auto* type:{"azure.script","azure.managed-script"}){
            const auto component=panel.view().componentData(selected->id,type);
            if(component.is_null())continue;
            const auto asset=component.value("asset",std::string());
            if(asset.empty())continue;
            ImGui::PushID(type);ImGui::TextWrapped("%s",asset.c_str());
            if(ui::button("Reveal source"))session_->edit("script.reveal",{{"asset",asset}});observeWidget(std::string("script.reveal.")+type);ImGui::PopID();
        }
    }
    ImGui::TextWrapped("Lua sources attach through the Script component. Managed sources use the registered build and deployment workflow.");
    ImGui::End();
}
}
#endif
